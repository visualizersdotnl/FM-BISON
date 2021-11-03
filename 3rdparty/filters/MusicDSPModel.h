
// ----------------------------------------------------------------------------------------------------
//
// Modified for FM. BISON
//
// - No external dependencies (including LadderBase (class))
// - Stereo support
// - Replaced MOOG_PI with SFM::kPI
// - Added SetParameters()
// - Added soft clip to prevent blowing up with samples outside of [-1..1] range
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
		memset(m_stage[0], 0, 4*sizeof(float));
		memset(m_stage[1], 0, 4*sizeof(float));
		memset(m_delay[0], 0, 4*sizeof(float));
		memset(m_delay[1], 0, 4*sizeof(float));
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
	SFM_INLINE void SetResonance(float resonance)
	{
		SFM_ASSERT(resonance >= 0.f && resonance <= 1.f);
		m_resonance = resonance * (t2 + 6.f*t1) / (t2 - 6.f*t1);
	}

	SFM_INLINE void SetCutoff(float cutoff, float resonance)
	{
		m_cutoff = 2.f * cutoff/m_sampleRate;

		p = m_cutoff * (1.8f - 0.8f * m_cutoff);
		k = 2.f * sinf(m_cutoff*SFM::kPI*0.5f) - 1.f;
		t1 = (1.f - p) * 1.386249f;
		t2 = 12.f + t1*t1;

		SetResonance(resonance);
	}

	SFM_INLINE void Apply(float &sample, float *stage, float *delay)
	{
		const float x = /* sample*m_drive - m_resonance*stage[3]; */ SFM::ultra_tanhf(sample*m_drive - m_resonance*stage[3]);

		// Four cascaded one-pole filters (bilinear transform)
		stage[0] = x * p + delay[0]  * p - k * stage[0];
		stage[1] = stage[0] * p + delay[1] * p - k * stage[1];
		stage[2] = stage[1] * p + delay[2] * p - k * stage[2];
		stage[3] = stage[2] * p + delay[3] * p - k * stage[3];
		
		// Clipping band-limited sigmoid
		stage[3] -= (stage[3] * stage[3] * stage[3]) / 6.f;
			
		delay[0] = x;
		delay[1] = stage[0];
		delay[2] = stage[1];
		delay[3] = stage[2];

		sample = stage[3];

		// Filter still in working order?
		SFM::FloatAssert(sample);
	}

	const unsigned m_sampleRate;

	float m_resonance;
	float m_cutoff;
	float m_drive = 1.f;

	float m_stage[2][4];
	float m_delay[2][4];

	float p;
	float k;
	float t1;
	float t2;
};
