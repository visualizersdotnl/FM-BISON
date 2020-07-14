	
/*
	FM. BISON hybrid FM synthesis -- One-pole filters (primarily for (MIDI) input filtering).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- LPF, HPF, cascaded LPF
	- Low cut (stereo)
	- DC blocker (stereo)

	There are more single pole filters in this codebase but they usually have a special purpose which
	makes their implementation a tad different (for example see synth-reverb.h), so I chose not to
	include those here but rather keep them alongside the code that uses them.

	I also tend to differ a bit from the standard DSP nomenclature in regard to variable naming.
	I'll try to refrain from doing this in the future (25/05/2020).
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	/* Lowpass */

	class SinglePoleLPF
	{
	public:
		SinglePoleLPF(float Fc = 1.f) :
			m_gain(1.f)
,			m_cutoff(0.f)
,			m_feedback(0.f)
	{
		SetCutoff(Fc);
	}

	void Reset(float value)
	{
		m_feedback = value;
	}

	void SetCutoff(float Fc)
	{
		SFM_ASSERT(Fc >= 0.f && Fc <= 1.f);
		m_cutoff = expf(-2.f*kPI*Fc);
		m_gain = 1.f-m_cutoff;
	}

	SFM_INLINE float Apply(float input)
	{
		m_feedback = input*m_gain + m_feedback*m_cutoff;
		return m_feedback;
	}

	SFM_INLINE float Get() const 
	{
		return m_feedback;
	}

	protected:
		float m_gain;
		float m_cutoff;

		float m_feedback;
	};

	/* Highpass */

	class SinglePoleHPF
	{
	public:
		SinglePoleHPF(float Fc = 1.f) :
			m_gain(1.f)
,			m_cutoff(0.f)
,			m_feedback(0.f)
	{
		SetCutoff(Fc);
	}

	void Reset(float value)
	{
		m_feedback = value;
	}

	void SetCutoff(float Fc)
	{
		SFM_ASSERT(Fc >= 0.f && Fc <= 1.f);
		m_cutoff = -expf(-2.f*kPI*(0.5f-Fc));
		m_gain = 1.f+m_cutoff;
	}

	SFM_INLINE float Apply(float input)
	{
		m_feedback = input*m_gain + m_feedback*m_cutoff;
		return m_feedback;
	}

	SFM_INLINE float Get() const 
	{
		return m_feedback;
	}

	protected:
		float m_gain;
		float m_cutoff;

		float m_feedback;
	};

	/* Cascaded SinglePoleLPF */

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

	/* Low cut (stereo) */

	class LowBlocker 
	{
	public:
		LowBlocker(float lowCutHz, unsigned sampleRate)
		{
			SFM_ASSERT(lowCutHz > 0.f && sampleRate > 0);
			
			const float Fc = lowCutHz/sampleRate;
			m_filterL.SetCutoff(Fc);
			m_filterR.SetCutoff(Fc);
		}
	
		~LowBlocker() {}

		SFM_INLINE void Reset()
		{
			m_filterL.Reset(0.f);
			m_filterR.Reset(0.f);
		}
	
		SFM_INLINE void Apply(float& left, float& right)
		{
			left =  m_filterL.Apply(left);
			right = m_filterR.Apply(right);
		}

	private:
		SinglePoleHPF m_filterL, m_filterR;
	};

	/* DC blocker (stereo) */

	class StereoDCBlocker
	{
	public:
		StereoDCBlocker() {}
		~StereoDCBlocker() {}

		SFM_INLINE void Apply(float &sampleL, float &sampleR)
		{
			constexpr float R = 0.995f;
			const float outL = sampleL - m_prevSample[0] + R*m_feedback[0];
			const float outR = sampleR - m_prevSample[1] + R*m_feedback[1];

			m_prevSample[0] = sampleL;
			m_prevSample[1] = sampleR;

			m_feedback[0] = outL;
			m_feedback[1] = outR;

			sampleL = outL;
			sampleR = outR;
		}

	private:
		float m_prevSample[2] = { 0.f, 0.f };
		float m_feedback[2]   = { 0.f, 0.f };
	};
}
