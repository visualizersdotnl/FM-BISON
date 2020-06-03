
/*
	FM. BISON hybrid FM synthesis -- Signal follower, A/R follower, RMS detector.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"
#include "synth-one-pole-filters.h"

namespace SFM
{
	class SignalFollower
	{
	public:
		SignalFollower(unsigned sampleRate, float MS = 1.f) :
			m_sampleRate(sampleRate)
		{
			SetTimeCoeff(MS);
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
		const unsigned m_sampleRate;

		float m_timeCoeff;
	};

	class AttackReleaseFollower
	{
	public:
		AttackReleaseFollower(unsigned sampleRate, float attackMS = 10.f, float releaseMS = 100.f) :
			m_attEnv(sampleRate, attackMS)
,			m_relEnv(sampleRate, releaseMS)
		{
		}

		SFM_INLINE void SetAttack(float MS)
		{
			m_attEnv.SetTimeCoeff(MS);
		}

		SFM_INLINE void SetRelease(float MS)
		{
			m_relEnv.SetTimeCoeff(MS);
		}

		SFM_INLINE float Apply(float sample, float &state)
		{
			if (sample > state)
				// Attack
				m_attEnv.Apply(sample, state);
			else
				// Release
				m_relEnv.Apply(sample, state);

			return state;
		}

	private:
		SignalFollower m_attEnv;
		SignalFollower m_relEnv;
	};

	// Use RMS to calculate signal dB
	class RMSDetector
	{
	public:
		RMSDetector(unsigned sampleRate, float lengthInSec) :
			m_numSamples(unsigned(sampleRate*lengthInSec))
		{
			SFM_ASSERT(m_numSamples > 0);
		}

		float Run(float sampleL, float sampleR)
		{
			// Pick rectified max. & raise
			const float rectMax    = std::max<float>(fabsf(sampleL), fabsf(sampleR));
			const float samplePow2 = rectMax*rectMax;
			
			// Pop tail
			if (m_buffer.size() == m_numSamples)
				m_buffer.pop_front();
			
			// Add head
			m_buffer.emplace_back(samplePow2);

			// FIXME: this might be too slow for larger windows
			float sum = 0.f;
			for (auto value : m_buffer)
				sum += value;
			
			const float RMS = sqrtf(sum/m_numSamples);
			FloatAssert(RMS);

			return RMS;
		}

	private:
		const unsigned m_numSamples;
		std::deque<float> m_buffer;
	};
}
