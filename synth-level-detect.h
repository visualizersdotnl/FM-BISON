
/*
	FM. BISON hybrid FM synthesis -- Peak & RMS detection.
	(C) njdewit technologies (visualizers.nl) & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"
#include "synth-signal-follower.h"
#include "synth-delay-line.h"

namespace SFM
{
	class RMS
	{
	public:
		RMS(unsigned sampleRate, float lengthInSec /* Window size */) :
			m_numSamples(unsigned(sampleRate*lengthInSec))
,			m_line(m_numSamples)
		{
			SFM_ASSERT(m_numSamples > 0);

			Reset();
		}

		~RMS() {}
	
	private:
		// Inserts new sample in circular buffer
		SFM_INLINE void Add(float sampleL, float sampleR)
		{
			// Pick rectified max. & raise
			const float rectMax = GetRectifiedMaximum(sampleL, sampleR);
			const float maxPow2 = rectMax*rectMax;
			FloatAssert(maxPow2);
			
			// Write to delay line
			m_line.Write(maxPow2);

			// Add to and subtract last from sum
			m_sum += maxPow2;
			m_sum -= m_line.ReadNormalized(1.f);

			// Snap to zero
			if (m_sum <= kEpsilon)
				m_sum = 0.f;
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
			if (0.f == m_sum)
				return kInfdB;

			const float RMS = sqrtf(m_sum/m_numSamples);
			return Lin2dB(RMS);
		}

		void Reset()
		{
			m_line.Reset();
		}

	private:
		const unsigned m_numSamples;

		DelayLine m_line;
		float m_sum = 0.f;
	};

	class Peak
	{
	public:
		Peak(unsigned sampleRate, float attackInSec) :
			m_peakEnv(sampleRate, attackInSec*1000.f /* MS used as unit in synth-signal-follower.h (see Github issue) */)
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
