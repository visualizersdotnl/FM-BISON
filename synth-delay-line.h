
/*
	FM. BISON hybrid FM synthesis -- Fractional delay line w/feeedback.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"
// #include "synth-one-pole-filters.h"

namespace SFM
{
	class DelayLine
	{
	public:
		DelayLine(size_t size, double Q = 0.025 /* Least possible Q for SVF */) :
			m_size(size)
,			m_buffer((float *) mallocAligned(size * sizeof(float), 16))
,			m_writeIdx(0)
,			m_Q(Q)
,			m_curSize(size)
		{
			Reset();
		}

		DelayLine(unsigned sampleRate, float lenghtInSec, float feedbackCutoffHz = 1.f) :
			DelayLine(size_t(sampleRate*lenghtInSec)) 
		{
			if (1.f == feedbackCutoffHz)
				m_feedbackLPF.updateCoefficients(CutoffToHz(1.f, sampleRate/2), m_Q, SvfLinearTrapOptimised2::LOW_PASS_FILTER, sampleRate);
			else
				m_feedbackLPF.updateCoefficients(feedbackCutoffHz, m_Q, SvfLinearTrapOptimised2::LOW_PASS_FILTER, sampleRate);
		}

		~DelayLine()
		{
			freeAligned(m_buffer);
		}

		void Reset()
		{
			memset(m_buffer, 0, m_size*sizeof(float));
			m_feedbackLPF.resetState();
		}

		void Resize(size_t numSamples)
		{
			Reset();
			
			SFM_ASSERT(numSamples > 0 && numSamples <= m_size);
			m_curSize = numSamples;

			m_writeIdx = 0;
		}

		SFM_INLINE void SetFeedbackCutoff(float cutoffHz, unsigned sampleRate)
		{
			SFM_ASSERT(cutoffHz >= 0.f && cutoffHz <= sampleRate/2);
			m_feedbackLPF.updateCoefficients(cutoffHz, m_Q, SvfLinearTrapOptimised2::LOW_PASS_FILTER, sampleRate);
		}

		SFM_INLINE void Write(float sample)
		{
			const unsigned index = m_writeIdx % m_curSize;
			m_buffer[index] = sample;
			++m_writeIdx;
		}

		// For filtered feedback path (call after Write())
		SFM_INLINE void WriteFeedback(float sample, float feedback)
		{
			SFM_ASSERT(feedback >= 0.f && feedback <= 1.f);
			const unsigned index = (m_writeIdx-1) % m_curSize;
			sample = sample*feedback;
			m_feedbackLPF.tickMono(sample);
			m_buffer[index] += sample;
		}

		// Delay is specified in samples relative to sample rate
		// Write first, then read
		SFM_INLINE float Read(float delay)
		{
			const size_t from = (m_writeIdx-1-int(delay)) % m_curSize;
			const size_t to   = (from > 0) ? from-1 : m_curSize-1;
			const float fraction = fracf(delay);
			const float A = m_buffer[from];
			const float B = m_buffer[to];
			const float value = lerpf<float>(A, B, fraction);
			return value;
		}

		// Same as above, but without interpolation
		SFM_INLINE float ReadNearest(int delay)
		{
			const size_t index = (m_writeIdx-1-delay) % m_curSize;
			return m_buffer[index];
		}

		size_t size() const { return m_size; }

	private:
		const size_t m_size;
		float *m_buffer;
		unsigned m_writeIdx;
		const double m_Q;

		size_t m_curSize;

//		LowpassFilter m_feedbackLPF;
		SvfLinearTrapOptimised2 m_feedbackLPF;
	};
}
