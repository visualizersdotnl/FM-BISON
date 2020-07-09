
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

// Based on an implementation by Magnus Jonsson
// https://github.com/magnusjonsson/microtracker (unlicense)

class MicrotrackerMoog
{
public:
	MicrotrackerMoog(unsigned sampleRate) :
		m_sampleRate(sampleRate)
	{
		Reset();

		SetCutoff(1000.f);
		SetResonance(0.1f);
	}

	~MicrotrackerMoog() {}

	SFM_INLINE void Reset()
	{
		p0[0] = p1[0] = p2[0] = p3[0] = p32[0] = p33[0] = p34[0] = 0.0;
		p0[1] = p1[1] = p2[1] = p3[1] = p32[1] = p33[1] = p34[1] = 0.0;
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
		Apply(0, left);
		Apply(1, right);
	}

private:
	SFM_INLINE void SetResonance(float resonance)
	{
		SFM_ASSERT(resonance >= 0.f && resonance <= 1.f);
		m_resonance = resonance;
	}

	SFM_INLINE void SetCutoff(float cutoff)
	{
		m_cutoff = cutoff * 2.f * M_PI / m_sampleRate;
		m_cutoff = std::min<double>(m_cutoff, 1.0);
	}

	SFM_INLINE void Apply(unsigned iChannel, float &sample)
	{
		SFM_ASSERT(iChannel < 2);

		const double k = m_resonance*4.0;

		// Coefficients optimized using differential evolution
		// to make feedback gain 4.0 correspond closely to the
		// border of instability, for all values of omega.
		const double out = p3[iChannel]*0.360891 + p32[iChannel]*0.417290 + p33[iChannel]*0.177896 + p34[iChannel]*0.0439725;

		p34[iChannel] = p33[iChannel];
		p33[iChannel] = p32[iChannel];
		p32[iChannel] = p3[iChannel];

		p0[iChannel] += (SFM::fast_tanh(sample*m_drive - k*out) - SFM::fast_tanh(p0[iChannel])) * m_cutoff;
		p1[iChannel] += (SFM::fast_tanh(p0[iChannel])  - SFM::fast_tanh(p1[iChannel])) * m_cutoff;
		p2[iChannel] += (SFM::fast_tanh(p1[iChannel])  - SFM::fast_tanh(p2[iChannel])) * m_cutoff;
		p3[iChannel] += (SFM::fast_tanh(p2[iChannel])  - SFM::fast_tanh(p3[iChannel])) * m_cutoff;

		sample = float(out);

		// Filter still in working order?
		SFM::SampleAssert(sample);
	}

	const unsigned m_sampleRate;

	double m_resonance;
	double m_cutoff;
	double m_drive = 1.0;

	double p0[2];
	double p1[2];
	double p2[2];
	double p3[2];
	double p32[2];
	double p33[2];
	double p34[2];
};
