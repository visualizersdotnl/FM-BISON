
// ----------------------------------------------------------------------------------------------------
//
// Modified for FM. BISON
//
// - No external dependencies (including LadderBase (class))
// - Stereo support
// - Replaced MOOG_PI with M_PI
// - Added SetParameters()
// - Misc. minor modifications 
//
// - Like most audio/DSP code out there it's messy, I won't fix that
//
// ----------------------------------------------------------------------------------------------------

#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

#include "../../synth-global.h"

/*
This class implements Tim Stilson's MoogVCF filter
using 'compromise' poles at z = -0.3

Several improments are built in, such as corrections
for cutoff and resonance parameters, removal of the
necessity of the separation table, audio rate update
of cutoff and resonance and a smoothly saturating
tanh() function, clamping output and creating inherent
nonlinearities.

This code is Unlicensed (i.e. public domain); in an email exchange on
4.21.2018 Aaron Krajeski stated: "That work is under no copyright. 
You may use it however you might like."

Source: http://song-swap.com/MUMT618/aaron/Presentation/demo.html
*/

class KrajeskiMoog
{
public:	
    KrajeskiMoog(unsigned sampleRate) :
		m_sampleRate(sampleRate)
	{
		SFM_ASSERT(sampleRate > 0);

		Reset();
		
		m_drive = 1.0;
		m_gComp = 1.0;
		
		SetCutoff(1000.f);
		SetResonance(0.1f);
	}
	
	~KrajeskiMoog() {}

	SFM_INLINE void Reset()
	{
		memset(m_state, 0, 2*5*sizeof(double));
		memset(m_delay, 0, 2*5*sizeof(double));
	}

	SFM_INLINE void SetParameters(float cutoff, float resonance, float drive /* Gain (linear) */)
	{
		SetCutoff(cutoff);
		SetResonance(resonance);

		SFM_ASSERT(drive >= 0.f);
		m_drive = drive;
	}

	SFM_INLINE void Apply(float &left, float &right)
	{
		Apply(left,  m_state[0], m_delay[0]);
		Apply(right, m_state[1], m_delay[1]);
	}
	
private:
	SFM_INLINE void SetResonance(float resonance)
	{
		SFM_ASSERT(resonance >= 0.f && resonance <= 1.f);

//		m_gRes = m_resonance * (1.0029 + 0.0526 * m_wc - 0.926 * pow(m_wc, 2) + 0.0218 * pow(m_wc, 3));

		const double wc2 = m_wc*m_wc;
		const double wc3 = wc2*m_wc;

		m_gRes = resonance * (1.0029 + 0.0526 * m_wc - 0.926 * wc2 + 0.0218 * wc3);
	}
	
	SFM_INLINE void SetCutoff(float cutoff)
	{
		m_wc = 2 * M_PI * cutoff / m_sampleRate;

//		m_g = 0.9892 * m_wc - 0.4342 * pow(m_wc, 2) + 0.1381 * pow(m_wc, 3) - 0.0202 * pow(m_wc, 4);

		const double wc2 = m_wc*m_wc;
		const double wc3 = wc2*m_wc;
		const double wc4 = wc3*m_wc;

		m_g = 0.9892 * m_wc - 0.4342 * wc2 + 0.1381 * wc3 - 0.0202 * wc4;
	}

	SFM_INLINE void Apply(float &sample, double state[], double delay[])
	{
		state[0] = tanh(m_drive * (sample - 4.0 * m_gRes * (state[4] - m_gComp * sample)));

		state[1] = m_g * (0.3/1.3 * state[0] + 1.0/1.3 * delay[0] - state[1]) + state[1];
		delay[0] = state[0];

		state[2] = m_g * (0.3/1.3 * state[1] + 1.0/1.3 * delay[1] - state[2]) + state[2];
		delay[1] = state[1];

		state[3] = m_g * (0.3/1.3 * state[2] + 1.0/1.3 * delay[2] - state[3]) + state[3];
		delay[2] = state[2];

		state[4] = m_g * (0.3/1.3 * state[3] + 1.0/1.3 * delay[3] - state[4]) + state[4];
		delay[3] = state[3];

		sample = float(state[4]);

		// Filter still in working order?
		SFM::SampleAssert(sample);
	}

	const unsigned m_sampleRate;
	
	double m_state[2][5]; // 2 channels (stereo)
	double m_delay[2][5]; //

	double m_wc;    // The angular frequency of the cutoff
	double m_g;     // A derived parameter for the cutoff frequency
	double m_gRes;  // A similar derived parameter for resonance
	double m_gComp; // Compensation factor
	double m_drive; // A parameter that controls intensity of non-linearities
};
