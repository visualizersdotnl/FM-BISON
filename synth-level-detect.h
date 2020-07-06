
/*
	FM. BISON hybrid FM synthesis -- Peak & RMS detection.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- Evaluate crest detection (should be easy given we have RMS and Peak)
*/

#pragma once

#include "synth-global.h"
#include "synth-sidechain-envelope.h"

namespace SFM
{
	class RMS
	{
	public:
		RMS(unsigned sampleRate, float lengthInSec /* Window size */) :
			m_numSamples(unsigned(sampleRate*lengthInSec))
,			m_buffer(new float[m_numSamples])
		{
			SFM_ASSERT(m_numSamples > 0);

			Reset();
		}

		~RMS()
		{
			delete[] m_buffer;
		}
	
	private:
		// Inserts new sample in circular buffer
		SFM_INLINE void Add(float sampleL, float sampleR)
		{
			// Pick rectified max. & raise
			const float rectMax    = GetRectifiedMaximum(sampleL, sampleR);
			const float samplePow2 = rectMax*rectMax;

			FloatAssert(samplePow2);

			// Store in (circular) buffer
			m_buffer[m_writeIdx++] = samplePow2;
			m_writeIdx %= m_numSamples;
		}
	
	public:
		// Does the above and returns the RMS in dB
		SFM_INLINE float Run(float sampleL, float sampleR)
		{
			Add(sampleL, sampleR);
			return GetdB();
		}

		// Calculate RMS and return dB
		SFM_INLINE float GetdB() const
		{
			float sum = 0.f;
			for (unsigned iSample = 0; iSample < m_numSamples; ++iSample)
				sum += m_buffer[iSample];

			const float RMS = sqrtf(sum/m_numSamples);
			FloatAssert(RMS);

			return (0.f != RMS) ? Lin2dB(RMS) : kInfdB;
		}

		void Reset()
		{
			memset(m_buffer, 0, m_numSamples*sizeof(float));
			m_writeIdx = 0;
		}

	private:
		const unsigned m_numSamples;

		float *m_buffer = nullptr;
		unsigned m_writeIdx = 0;
	};

	class Peak
	{
	public:
		Peak(unsigned sampleRate, float attackInSec) :
			m_peakEnv(sampleRate, attackInSec*1000.f /* MS used as unit in synth-sidechain-envelope.h (see Github issue) */)
		{}

		SFM_INLINE void Reset()
		{
			m_peak = 0.f;
		}

		SFM_INLINE float Run(float sampleL, float sampleR)
		{
			// Pick rect. max.
			const float rectMax = GetRectifiedMaximum(sampleL, sampleR);
			
			// Apply & return
			m_peakEnv.Apply(rectMax, m_peak);
			return GetdB();
		}

		SFM_INLINE float GetdB() const
		{
			const float peakEnv = m_peak;
			return (0.f != peakEnv) ? Lin2dB(peakEnv) : kInfdB;
		}

		SFM_INLINE void SetAttack(float attackInSec)
		{
			SFM_ASSERT(attackInSec > 0.f);
			m_peakEnv.SetTimeCoeff(attackInSec*1000.f);
		}

	private:
		SignalFollower m_peakEnv;
		float m_peak = 0.f;
	};
}
