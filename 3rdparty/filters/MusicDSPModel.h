
// ----------------------------------------------------------------------------------------------------
//
// Modified for FM. BISON
//
// FIXME: modify to single precision
//
// - No external dependencies (including LadderBase (class))
// - Stereo support
// - Replaced MOOG_PI with M_PI
// - Added SetParameters()
// - Added soft clip (SFM::ultra_tanh()) to prevent blowing up with samples outside of [-1..1] range
// - Misc. minor modifications 
//
// I can't say I'm a big fan of how most DSP code is written but I'll try to keep it as-is.
//
// ----------------------------------------------------------------------------------------------------

#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

#include "../../synth-global.h"

// This file is unlicensed and uncopyright as found at:
// http://www.musicdsp.org/showone.php?id=24
// Considering how widely this same code has been used in ~100 projects on GitHub with 
// various licenses, it might be reasonable to suggest that the license is CC-BY-SA

class MusicDSPMoog
{
public:
	MusicDSPMoog(unsigned sampleRate) :
		m_sampleRate(sampleRate)
	{
		Reset();
		
		SetParameters(1000.f, 0.1f, 1.f);
	}

	~MusicDSPMoog() {}

	SFM_INLINE void Reset()
	{
		memset(m_stage[0], 0, 4*sizeof(double));
		memset(m_stage[1], 0, 4*sizeof(double));
		memset(m_delay[0], 0, 4*sizeof(double));
		memset(m_delay[1], 0, 4*sizeof(double));
	}

	SFM_INLINE void SetParameters(float cutoff, float resonance, float drive /* Gain (linear) */)
	{
		SetCutoff(cutoff, resonance); // Resonance relies on cutoff

		SFM_ASSERT(drive >= 0.f);
		m_drive = drive;
	}

	SFM_INLINE void Apply(float &left, float &right)
	{
		Apply(left, m_stage[0], m_delay[0]);
		Apply(right, m_stage[1], m_delay[1]);
	}

private:
	SFM_INLINE void SetResonance(double resonance)
	{
		SFM_ASSERT(resonance >= 0.f && resonance <= 1.f);
		m_resonance = resonance * (t2 + 6.0 * t1) / (t2 - 6.0 * t1);
	}

	SFM_INLINE void SetCutoff(float cutoff, float resonance)
	{
		m_cutoff = 2.0 * cutoff / m_sampleRate;

		p = m_cutoff * (1.8 - 0.8 * m_cutoff);
		k = 2.0 * sin(m_cutoff * M_PI * 0.5) - 1.0;
		t1 = (1.0 - p) * 1.386249;
		t2 = 12.0 + t1 * t1;

		SetResonance(resonance);
	}

	SFM_INLINE void Apply(float &sample, double *stage, double *delay)
	{
		// Without the soft clip it's prone to blow up in a path that doesn't necessarily adhere to [-1..1] at that point
		double x = SFM::ultra_tanh(sample*m_drive - m_resonance*stage[3]);

		// Four cascaded one-pole filters (bilinear transform)
		stage[0] = x * p + delay[0]  * p - k * stage[0];
		stage[1] = stage[0] * p + delay[1] * p - k * stage[1];
		stage[2] = stage[1] * p + delay[2] * p - k * stage[2];
		stage[3] = stage[2] * p + delay[3] * p - k * stage[3];
		
		// Clipping band-limited sigmoid
		stage[3] -= (stage[3] * stage[3] * stage[3]) / 6.0;
			
		delay[0] = x;
		delay[1] = stage[0];
		delay[2] = stage[1];
		delay[3] = stage[2];

		sample = float(stage[3]); // FIXME!

		// Filter still in working order?
		SFM::FloatAssert(sample);
	}

	const unsigned m_sampleRate;

	double m_resonance;
	double m_cutoff;
	double m_drive = 1.0;

	double m_stage[2][4];
	double m_delay[2][4];

	double p;
	double k;
	double t1;
	double t2;
};
