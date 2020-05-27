
/*
	FM. BISON hybrid FM synthesis -- Signal follower, A/R follower, RMS detector.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"

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
,			m_buffer(m_numSamples)
,			m_sum(0.f)
		{
			SFM_ASSERT(m_numSamples > 0);
		}

		float Run(float sampleL, float sampleR)
		{
			// Mix down to monaural & raise
			const float monaural = sampleL*0.5f + sampleR*0.5f;
			const float samplePow2 = monaural*monaural;

			// Write/Read/Sum	
			m_buffer.Write(samplePow2);
			const float tail = m_buffer.IsFull() ? m_buffer.Read() : 0.f;
			m_sum -= tail;
			m_sum += samplePow2;
			
			// Voila!
			const float RMS = sqrtf(fabsf(m_sum)/m_numSamples);
			FloatAssert(RMS);
				
			return RMS;
		}

	private:
		const unsigned m_numSamples;
		RingBuffer m_buffer;
		float m_sum;
	};
}
