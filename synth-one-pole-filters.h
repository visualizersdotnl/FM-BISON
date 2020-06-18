	
/*
	FM. BISON hybrid FM synthesis -- One-pole filters (primarily for (MIDI) input filtering).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Ref.: https://www.earlevel.com/main/2012/12/15/a-one-pole-filter/

	- LP, HP 
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

	class LowpassFilter
	{
	public:
		LowpassFilter(float Fc = 1.f) :
			m_gain(1.f)
,			m_cutoff(0.f)
,			m_feedback(0.f)
	{
		SetCutoff(Fc);
	}
		
	virtual ~LowpassFilter() {}

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

	class HighpassFilter
	{
	public:
		HighpassFilter(float Fc = 1.f) :
			m_gain(1.f)
,			m_cutoff(0.f)
,			m_feedback(0.f)
	{
		SetCutoff(Fc);
	}
		
	virtual ~HighpassFilter() {}

	void Reset(float value)
	{
		m_feedback = value;
	}

	void SetCutoff(float Fc)
	{
		SFM_ASSERT(Fc >= 0.f && Fc <= 1.f);
		m_cutoff = -expf(-2.f*kPI*(1.f-Fc));
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

	/* Cascaded lowpass (12dB) */

	class LowpassFilter12dB
	{
	public:
		LowpassFilter12dB(float Fc = 1.f) :
			m_filterA(Fc)
,			m_filterB(Fc) {}

	void Reset(float value)
	{
		m_filterA.Reset(value);
		m_filterB.Reset(value);

		m_current = 0.f;
	}

	void SetCutoff(float Fc)
	{
		m_filterA.SetCutoff(Fc);
		m_filterB.SetCutoff(Fc);
	}

	SFM_INLINE float Apply(float input)
	{
		m_current = m_filterB.Apply(m_filterA.Apply(input));
		return m_current;
	}

	SFM_INLINE float Get() const
	{
		return m_current;
	}

	private:
		LowpassFilter m_filterA;
		LowpassFilter m_filterB;

		float m_current = 0.f;
	};

	/* Low cut (stereo) */
	
	class LowBlocker 
	{
	public:
		LowBlocker(float lowCutHz, unsigned sampleRate) :
			m_feedbackL(0.f), m_feedbackR(0.f)
		{
			SFM_ASSERT(lowCutHz > 0.f && sampleRate > 0);
			SetCutoff(lowCutHz/sampleRate);
		}
	
		~LowBlocker() {}

		SFM_INLINE void Reset()
		{
			m_feedbackL = m_feedbackR = 0.f;
		}
	
		SFM_INLINE void Apply(float& left, float& right)
		{
			m_feedbackL =  left*m_gain + m_feedbackL*m_cutoff;
			m_feedbackR = right*m_gain + m_feedbackR*m_cutoff;

			// Cut it off!
			left  -= m_feedbackL;
			right -= m_feedbackR;
		}

	private: 
		SFM_INLINE void SetCutoff(float Fc)
		{
			SFM_ASSERT(Fc >= 0.f && Fc <= 1.f);
			m_cutoff = expf(-2.f*kPI*Fc);
			m_gain = 1.f-m_cutoff;
		}

		float m_gain, m_cutoff;

		float m_feedbackL; 
		float m_feedbackR;
	};

	/* DC blocker (incl. stereo) */

	class DCBlocker
	{
	public:
		DCBlocker() {}
		~DCBlocker() {}

		SFM_INLINE void Reset()
		{
			m_prevSample = 0.f;
			m_feedback = 0.f;
		}

		SFM_INLINE float Apply(float sample)
		{
			constexpr float R = 0.995f;
			const float result = sample - m_prevSample + R*m_feedback;
			
			m_prevSample = sample;
			m_feedback = result;

			return result;
		}

		SFM_INLINE float Get() const 
		{
			return m_feedback;
		}

	private:
		float m_prevSample = 0.f;
		float m_feedback   = 0.f;
	};

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
		float m_prevSample[2] = { 0.f };
		float m_feedback[2]   = { 0.f };
	};
}
