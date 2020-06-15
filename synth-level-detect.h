
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
	// Stereo linking (and rectification)
	SFM_INLINE static float GetRectifiedMaximum(float sampleL, float sampleR)
	{
		return std::max<float>(fabsf(sampleL), fabsf(sampleR));
	}

	class RMS
	{
	public:
		RMS(unsigned sampleRate, float lengthInSec /* Window size */) :
			m_numSamples(unsigned(sampleRate*lengthInSec))
		{
			SFM_ASSERT(m_numSamples > 0);
		}

		// Feeds sample and pops tail if necessary
		SFM_INLINE void Add(float sampleL, float sampleR)
		{
			// Pick rectified max. & raise
			const float rectMax    = GetRectifiedMaximum(sampleL, sampleR);
			const float samplePow2 = rectMax*rectMax;

			// Pop tail
			if (m_buffer.size() == m_numSamples)
				m_buffer.pop_front();
			
			// Add head
			m_buffer.emplace_back(samplePow2);
		}

		// Does the above and returns the RMS in dB
		SFM_INLINE float Run(float sampleL, float sampleR)
		{
			Add(sampleL, sampleR);
			return GetdB();
		}

		// Calculate RMS and return dB
		SFM_INLINE float GetdB() const
		{
			// FIXME: this might be too slow for larger windows
			float sum = 0.f;
			for (auto value : m_buffer)
				sum += value;
			
			const float RMS = sqrtf(sum/m_numSamples);
			FloatAssert(RMS);

			return (0.f != RMS) ? Lin2dB(RMS) : kMinVolumedB;
		}

		void Reset()
		{
			m_buffer.clear();
		}

	private:
		const unsigned m_numSamples;
		std::deque<float> m_buffer;
	};

	class Peak
	{
	public:
		Peak(unsigned sampleRate, float attackInSec, float releaseInSec) :
			m_peakEnv(sampleRate, attackInSec, releaseInSec)
		{
		}

		SFM_INLINE void Reset()
		{
			m_peakEnv.Reset();
		}

		SFM_INLINE float Run(float sampleL, float sampleR)
		{
			// Pick rect. max.
			const float rectMax = GetRectifiedMaximum(sampleL, sampleR);
			
			// Apply & return
			return m_peakEnv.Apply(rectMax);
			return GetdB();
		}

		SFM_INLINE float GetdB() const
		{
			const float peakEnv = m_peakEnv.Get();
			return (0.f != peakEnv) ? Lin2dB(peakEnv) : kMinVolumedB;
		}

	private:
		FollowerEnvelope m_peakEnv;
	};
}
