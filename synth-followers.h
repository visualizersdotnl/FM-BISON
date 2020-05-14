
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
,				m_buffer((float *) mallocAligned(m_numSamples * sizeof(float), 16))
,				m_writeIdx(0)
		{
			SFM_ASSERT(m_numSamples > 0);
				
			// Clear buffer
			memset(m_buffer, 0, m_numSamples * sizeof(float));
		}

		~RMSDetector()
		{
			freeAligned(m_buffer);
		}

		float Run(float sampleL, float sampleR)
		{
			// Mix down to monaural
			const float monaural = sampleL*0.5f + sampleR*0.5f;
				
			// Raise & write
			const unsigned index = m_writeIdx % m_numSamples;
			const float samplePow2 = monaural*monaural;
			m_buffer[index] = samplePow2;
			++m_writeIdx;
				
			// Calculate RMS (FIXME: use circular buffer & running sum)
			float sum = 0.f;
			for (unsigned iSample = 0; iSample < m_numSamples; ++iSample)
				sum += m_buffer[iSample];

			const float RMS = sqrtf(sum/m_numSamples);
				
			return RMS;
		}

	private:
		const unsigned m_numSamples;
		float *m_buffer;

		unsigned m_writeIdx;
	};
}
