
//
// Modified to fit FM. BISON:
// - Fitted to compile without dependencies
// - Stereo support
// - Unrolled loop
// - Added a few setup functions
// - Stability assertion
// - Optimized SetCutoff() & SetResonance()
// - Stereo processing loop (Process())
//
// Source: https://github.com/ddiakopoulos/MoogLadders/tree/master/src
//

#pragma once

#include "../synth-fast-tan.h" 
#include "../synth-helper.h"

#ifndef MOOG_PI
	#define MOOG_PI 3.14159265358979323846264338327950288
#endif

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
		SFM_ASSERT(sampleRate >= 0);

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

	SFM_INLINE void Apply(float& left, float& right)
	{
		Apply(left,  m_state[0], m_delay[0]);
		Apply(right, m_state[1], m_delay[1]);
	}

	void Process(float* samplesL, float* samplesR, unsigned numSamples)
	{
		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			float left = samplesL[iSample], right = samplesR[iSample];
			Apply(left, right);
			samplesL[iSample] += m_wetness*left;
			samplesR[iSample] += m_wetness*right;
		}
	}

	SFM_INLINE void SetParameters(float cutoff, float Q, float drive, float wetness = 1.f /* Set for Process() */)
	{
		SetCutoff(cutoff);
		SetResonance(Q);
		m_wetness = wetness;

		SFM_ASSERT(drive >= 0.f);
		m_drive = drive;
	}
	
private:
	SFM_INLINE void SetResonance(float r)
	{
		SFM_ASSERT(r >= 0.f && r <= SFM::kMaxPostFilterResonance);
		m_resonance = r;

		// Traded in favour of less pow() calls
//		m_gRes = m_resonance * (1.0029 + 0.0526 * m_wc - 0.926 * pow(m_wc, 2) + 0.0218 * pow(m_wc, 3));

		const double wc2 = m_wc*m_wc;
		const double wc3 = wc2*m_wc;

		m_gRes = m_resonance * (1.0029 + 0.0526 * m_wc - 0.926 * wc2 + 0.0218 * wc3);
	}
	
	SFM_INLINE void SetCutoff(float c)
	{
		// FIXME: assert?
		m_cutoff = c;

		m_wc = 2 * MOOG_PI * m_cutoff / m_sampleRate;

		// Traded in favour of less pow() calls
//		m_g = 0.9892 * m_wc - 0.4342 * pow(m_wc, 2) + 0.1381 * pow(m_wc, 3) - 0.0202 * pow(m_wc, 4);

		const double wc2 = m_wc*m_wc;
		const double wc3 = wc2*m_wc;
		const double wc4 = wc3*m_wc;

		m_g = 0.9892 * m_wc - 0.4342 * wc2 + 0.1381 * wc3 - 0.0202 * wc4;
	}

	SFM_INLINE void Apply(float& sample, double state[], double delay[])
	{
		state[0] = tanh(m_drive * (sample - 4.0 * m_gRes * (state[4] - m_gComp * sample)));
//		state[0] = SFM::fast_tanh(m_drive * (sample - 4.0 * m_gRes * (state[4] - m_gComp * sample)));
//		state[0] = SFM::ultra_tanh(m_drive * (sample - 4.0 * m_gRes * (state[4] - m_gComp * sample)));

		state[1] = m_g * (0.3/1.3 * state[0] + 1.0/1.3 * delay[0] - state[1]) + state[1];
		delay[0] = state[0];

		state[2] = m_g * (0.3/1.3 * state[1] + 1.0/1.3 * delay[1] - state[2]) + state[2];
		delay[1] = state[1];

		state[3] = m_g * (0.3/1.3 * state[2] + 1.0/1.3 * delay[2] - state[3]) + state[3];
		delay[2] = state[2];

		state[4] = m_g * (0.3/1.3 * state[3] + 1.0/1.3 * delay[3] - state[4]) + state[4];
		delay[3] = state[3];

		sample = float(state[4]);

		// Is filter still stable?
		SFM::SampleAssert(sample);
	}

	unsigned m_sampleRate;
	float    m_cutoff;
	float    m_resonance;
	float    m_wetness;
	
	// For 2 channels (stereo)
	double m_state[2][5];
	double m_delay[2][5];

	double m_wc;    // The angular frequency of the cutoff
	double m_g;     // A derived parameter for the cutoff frequency
	double m_gRes;  // A similar derived parameter for resonance
	double m_gComp; // Compensation factor
	double m_drive; // A parameter that controls intensity of non-linearities
};
