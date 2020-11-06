	
/*
	FM. BISON hybrid FM synthesis -- One-pole filters.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- LPF, cascaded LPF
	- DC blocker (stereo)
	- 17/07/2020 - I've adopted established DSP filter nomenclature (like I promised)

	There are more single pole filters in this codebase but they usually have a special purpose which
	makes their implementation a tad different (for example see synth-reverb.h), so I chose not to
	include those here but rather keep them alongside the code that uses them
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	/* LPF */

	class SinglePoleLPF
	{
	public:
		SinglePoleLPF(float Fc = 1.f) :
			m_a0(1.f)
,			m_b1(0.f)
,			m_z1(0.f)
	{
		SetCutoff(Fc);
	}

	void Reset(float value)
	{
		m_z1 = value;
	}

	void SetCutoff(float Fc)
	{
		SFM_ASSERT(Fc >= 0.f && Fc <= 1.f);
		m_b1 = expf(-2.f*kPI*Fc);
		m_a0 = 1.f-m_b1;
	}

	SFM_INLINE float Apply(float input)
	{
		return m_z1 = input*m_a0 + m_z1*m_b1;
	}

	SFM_INLINE float Get() const 
	{
		return m_z1;
	}

	private:
		float m_a0;
		float m_b1;
		float m_z1;
	};

	/* Cascaded LPF */

	class CascadedSinglePoleLPF
	{
	public:
		CascadedSinglePoleLPF(float Fc = 1.f) :
			m_filterA(Fc)
,			m_filterB(Fc) {}

	void Reset(float value)
	{
		m_filterA.Reset(value);
		m_filterB.Reset(value);
	}

	void SetCutoff(float Fc)
	{
		m_filterA.SetCutoff(Fc);
		m_filterB.SetCutoff(Fc);
	}

	SFM_INLINE float Apply(float input)
	{
		return m_filterB.Apply(m_filterA.Apply(input));
	}

	SFM_INLINE float Get() const
	{
		return m_filterB.Get();
	}

	private:
		SinglePoleLPF m_filterA;
		SinglePoleLPF m_filterB;
	};

	/* DC blocker (stereo) */

	class StereoDCBlocker
	{
	public:
		SFM_INLINE void Apply(float &sampleL, float &sampleR)
		{
			constexpr float R = 0.995f; // What "everyone" uses in a leaky integrator is 0.995
			const float outL = sampleL-m_prevSample[0] + R*m_feedback[0];
			const float outR = sampleR-m_prevSample[1] + R*m_feedback[1];

			m_prevSample[0] = sampleL;
			m_prevSample[1] = sampleR;

			m_feedback[0] = outL;
			m_feedback[1] = outR;

			sampleL = outL;
			sampleR = outR;
		}

	private:
		float m_prevSample[2] = { 0.f };
		float m_feedback[2]   = { 0.f };
	};
}
