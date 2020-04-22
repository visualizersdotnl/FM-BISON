
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Source that got me started: https://github.com/buosseph/JuceCompressor
	Thanks to Tammo Hinrichs for a few good tips!

	FIXME:
		- Create stereo processing loop including interpolation
		- Read up on compressors to beef up this rather simple implementation (check Will Pirkle's book)
*/

#pragma once

// #include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"

namespace SFM
{
	class Compressor
	{
	private:
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
		// FIXME: unused!
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

		class Gain
		{
		public:
			Gain(unsigned sampleRate, float attack, float release) :
				m_sampleRate(sampleRate)
			{
				Reset(attack, release);
			}

			~Gain() {}

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
			/* const */ float m_attack;
			/* const */ float m_release;
			
			// FIXME
			float m_outGain;
			double m_attackB0, m_releaseB0;
		};

	public:
		Compressor(unsigned sampleRate) :
			m_sampleRate(sampleRate)
,			m_detectorL(sampleRate)
,			m_detectorR(sampleRate)
//,			m_RMSDetector(sampleRate, 0.05f  /* 50MS: http://replaygain.hydrogenaud.io/proposal/rms_energy.html */)
,			m_RMSDetector(sampleRate, 0.005f /*  5MS: Reaper's compressor default                               */)
,			m_gainDyn(sampleRate, kDefCompAttack, kDefCompRelease)
		{
 			SetParameters(kDefCompPeakToRMS, kDefCompThresholddB, kDefCompKneedB, kDefCompRatio, kDefCompGaindB, kDefCompAttack, kDefCompRelease);
		}

		~Compressor() {}

		SFM_INLINE void SetParameters(float peakToRMS, float thresholddB, float kneedB, float ratio, float gaindB, float attack, float release)
		{
			SFM_ASSERT(peakToRMS >= 0.f && peakToRMS <= 1.f);
			SFM_ASSERT(thresholddB >= kMinCompThresholdB && thresholddB <= kMaxCompThresholdB);
			SFM_ASSERT(kneedB >= kMinCompKneedB && kneedB <= kMaxCompKneedB);
			SFM_ASSERT(ratio >= kMinCompRatio && ratio <= kMaxCompRatio);
			SFM_ASSERT(gaindB >= kMinCompGaindB && gaindB <= kMaxCompGaindB);
			SFM_ASSERT(attack >= kMinCompAttack && attack <= kMaxCompAttack);
			SFM_ASSERT(release >= kMinCompRelease && attack <= kMaxCompRelease);

			m_peakToRMS = peakToRMS;
			m_thresholddB = thresholddB;
			m_kneedB = kneedB;
			m_ratio = ratio;
			m_postGain = dBToGain(gaindB);

			m_gainDyn.SetAttack(attack);
			m_gainDyn.SetRelease(release);
		}

		SFM_INLINE void Apply(float &left, float &right)
		{
			// Detect peak
			const float peakOutL  = m_detectorL.Apply(left);
			const float peakOutR  = m_detectorR.Apply(right);
			const float peakSum   = fast_tanhf(peakOutL+peakOutR)*0.5f; // Soft clip peak sum sounds *good*
			const float RMS       = m_RMSDetector.Run(left, right);
			const float sum       = lerpf<float>(peakSum, RMS, m_peakToRMS);
			const float sumdB     = (0.f != sum) ? GainTodB(sum) : kMinCompThresholdB;

			// Crush it!
			float gaindB;
			float kneeBlend;
			if (sumdB < m_thresholddB)
			{
				gaindB = 0.f;
				kneeBlend = 0.f;
			}
			else
			{
				SFM_ASSERT(0.f != m_ratio);
				gaindB = -(sumdB-m_thresholddB) * (1.f - 1.f/m_ratio);
				kneeBlend = std::min<float>(sumdB-m_thresholddB, m_kneedB);
			}

			SFM_ASSERT(kneeBlend >= 0.f && kneeBlend <= m_kneedB);
			kneeBlend /= m_kneedB;
			gaindB = lerpf<float>(0.f, gaindB, kneeBlend*kneeBlend);

			gaindB = m_gainDyn.Apply(gaindB);
			const float gain = dBToGain(gaindB);
			
			// Apply w/gain
			left  = (left*gain)  * m_postGain;
			right = (right*gain) * m_postGain;
		}

	private:
		const unsigned m_sampleRate;

		PeakDetector m_detectorL, m_detectorR;
		RMSDetector m_RMSDetector;
		Gain m_gainDyn;

		// Parameters
		float m_peakToRMS;
		float m_thresholddB;
		float m_kneedB;
		float m_ratio;
		float m_postGain;
	};
}
