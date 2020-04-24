
/*
	FM. BISON hybrid FM synthesis -- Peak/RMS detector & gain shaper.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	class PeakDetector
	{
	public:
		PeakDetector(unsigned sampleRate) :
			m_sampleRate(sampleRate)
		{
			Reset();
		}

		~PeakDetector() {}

		SFM_INLINE float Apply(float sample)
		{
			const float sampleAbs = fabsf(sample);

			float B0;
			if (sampleAbs > m_peak)
				B0 = m_attackB0;
			else
				B0 = m_releaseB0;
				
			m_peak += B0*(sampleAbs-m_peak);

			return m_peak;
		}

	private:
		SFM_INLINE void Reset()
		{
			m_peak = 0.f;

			m_attackB0 = 1.f;
			m_A1 = expf(-1.f / (m_release*m_sampleRate));
			m_releaseB0 = 1.f-m_A1;
		}
    
		const unsigned m_sampleRate;
		const float m_release = 0.1f; // Tenth of a second (fixed)
			
		// Set on Reset()
		float m_attackB0, m_releaseB0, m_A1;

		float m_peak;
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
			// Mix down to monaural (soft clip)
			const float monaural = fast_atanf(sampleL+sampleR)*0.5f;
				
			// Raise & write
			const unsigned index = m_writeIdx % m_numSamples;
			const float samplePow2 = powf(monaural, 2.f);
			m_buffer[index] = samplePow2;
			++m_writeIdx;
				
			// Calculate RMS
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

	// Gain (envelope) shaper
	class GainShaper
	{
	public:
		GainShaper(unsigned sampleRate, float attack, float release) :
			m_sampleRate(sampleRate)
		{
			Reset(attack, release);
		}

		~GainShaper() {}

		SFM_INLINE void Reset(float attack, float release)
		{    
			m_outGain = 0.f;
			SetAttack(attack);
			SetRelease(release);
		}

		SFM_INLINE void SetAttack(float attack)
		{
			m_attack = attack;
			m_attackB0 = 1.0 - exp(-1.0 / (m_attack*m_sampleRate));
		}

		SFM_INLINE void SetRelease(float release)
		{
			m_release = release;
			m_releaseB0 = 1.0 - exp(-1.0 / (m_release*m_sampleRate));
		}

		SFM_INLINE float Apply(float gain)
		{
			double B0;
			if (gain < m_outGain)
				B0 = m_attackB0;
			else
				B0 = m_releaseB0;
				
			m_outGain += float(B0 * (gain-m_outGain));

			return m_outGain;
		}
		
	private:
		const unsigned m_sampleRate;
		float m_attack;
		float m_release;
			
		float m_outGain;
		double m_attackB0, m_releaseB0;
	};
}

