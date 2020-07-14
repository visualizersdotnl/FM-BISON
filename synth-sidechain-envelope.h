
/*
	FM. BISON hybrid FM synthesis -- Sidechain: signal follower & attack/release envelope.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	// This class is also used to apply slew to signal(s) and parameters
	// It's also the second implementation to date (15/06/2020) of this filter (see synth-one-pole-filters.h)
	class SignalFollower
	{
	public:
		SignalFollower() :
			m_sampleRate(1), m_timeCoeff(0.f) {}

		SignalFollower(unsigned sampleRate, float MS = 1.f)
		{
			SetSampleRate(sampleRate);
			SetTimeCoeff(MS);
		}

		~SignalFollower() {}

		SFM_INLINE void SetSampleRate(unsigned sampleRate)
		{
			SFM_ASSERT(sampleRate > 0);
			m_sampleRate = sampleRate;
		}			

		SFM_INLINE void SetTimeCoeff(float MS)
		{
			SFM_ASSERT(MS > 0.f);
			m_timeCoeff = expf(-1000.f / (MS*m_sampleRate));
		}

		SFM_INLINE float Apply(float sample, float &state)
		{
			state = sample + m_timeCoeff*(state-sample);
			return state;
		}

	private:	
		unsigned m_sampleRate;
		float m_timeCoeff;
	};

	class FollowerEnvelope
	{
	public:
		FollowerEnvelope() { Reset(); }

		FollowerEnvelope(unsigned sampleRate, float state, float attackMS = 10.f, float releaseMS = 100.f) :
			m_attEnv(sampleRate, attackMS)
,			m_relEnv(sampleRate, releaseMS)
,			m_state(state)
		{
		}

		SFM_INLINE void Reset(float value = 0.f)
		{
			m_state = value;
		}

		SFM_INLINE void SetSampleRate(unsigned sampleRate)
		{
			SFM_ASSERT(sampleRate > 0);

			m_attEnv.SetSampleRate(sampleRate);
			m_relEnv.SetSampleRate(sampleRate);
		}

		SFM_INLINE void SetAttack(float MS)
		{
			m_attEnv.SetTimeCoeff(MS);
		}

		SFM_INLINE void SetRelease(float MS)
		{
			m_relEnv.SetTimeCoeff(MS);
		}

		SFM_INLINE float Apply(float sample)
		{
			if (sample > m_state)
				// Attack
				m_attEnv.Apply(sample, m_state);
			else
				// Release
				m_relEnv.Apply(sample, m_state);

			return m_state;
		}

		// For cases a lower value actually means we're in attack phase
		SFM_INLINE float ApplyReverse(float sample)
		{
			if (sample < m_state)
				// Attack
				m_attEnv.Apply(sample, m_state);
			else
				// Release
				m_relEnv.Apply(sample, m_state);

			return m_state;
		}

		SFM_INLINE float ApplyStereo(float sampleL, float sampleR)
		{
			return Apply(GetRectifiedMaximum(sampleL, sampleR));
		}

		SFM_INLINE float Get() const
		{
			return m_state;
		}

	private:
		SignalFollower m_attEnv;
		SignalFollower m_relEnv;

		float m_state = 0.f;
	};
}
