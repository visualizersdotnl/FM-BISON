
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Source that got me started: https://github.com/buosseph/JuceCompressor

	FIXME:
		- Add soft knee (approx. 3dB?)
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
				m_releaseB0 = 1.f - exp(-1.0 / (m_release*m_sampleRate));
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
,			m_gainDyn(sampleRate, kDefCompAttack, kDefCompRelease)
		{
			SetParameters(kDefCompThresholddB, kDefCompKneedB, kDefCompLookaheadMS, kDefCompRatio, kDefCompAttack, kDefCompRelease);
		}

		~Compressor() {}

		SFM_INLINE void SetParameters(float thresholddB, float kneedB, float lookaheadMS, float ratio, float attack, float release)
		{
			SFM_ASSERT(thresholddB >= kMinCompThresholdB && thresholddB <= kMaxCompThresholdB);
			SFM_ASSERT(kneedB >= kMinCompKneedB && kneedB <= kMaxComKneedB);
			SFM_ASSERT(lookaheadMS >= kMinCompLookaheadMS && lookaheadMS <= kMaxCompLookaheadMS);
			SFM_ASSERT(ratio >= kMinCompRatio && ratio <= kMaxCompRatio);
			SFM_ASSERT(attack >= kMinCompAttack && attack <= kMaxCompAttack);
			SFM_ASSERT(release >= kMinCompRelease && attack <= kMaxCompRelease);

			m_thresholddB = thresholddB;
			m_kneedB = kneedB;
			m_lookaheadMS = lookaheadMS;
			m_ratio = ratio;

			m_gainDyn.SetAttack(attack);
			m_gainDyn.SetRelease(release);
		}

		SFM_INLINE void Apply(float &left, float &right)
		{
			m_peakOutL = m_detectorL.Apply(left);
			m_peakOutR = m_detectorR.Apply(right);
			m_peakSum = (m_peakOutL+m_peakOutR)*0.5f;
			m_peakSumdB = GainTodB(m_peakSum);
			
			float gaindB;
			float kneeBlend;
			if (m_peakSumdB < m_thresholddB)
			{
				gaindB = 0.f;
				kneeBlend = 0.f;
			}
			else
			{
				SFM_ASSERT(0.f != m_ratio);
				gaindB = -(m_peakSumdB-m_thresholddB) * (1.f - 1.f/m_ratio);
				kneeBlend = std::min<float>(m_peakSumdB-m_thresholddB, m_kneedB);
			}

			SFM_ASSERT(kneeBlend >= 0.f && kneeBlend <= m_kneedB);
			m_gaindB = lerpf<float>(0.f, gaindB, smoothstepf(kneeBlend/m_kneedB));

			m_gaindB = m_gainDyn.Apply(m_gaindB);
			m_gain   = dBToGain(m_gaindB);

			left  *= m_gain;
			right *= m_gain;
		}

	private:
		const unsigned m_sampleRate;

		PeakDetector m_detectorL, m_detectorR;
		float m_peakOutL, m_peakOutR, m_peakSum, m_peakSumdB;

		Gain m_gainDyn;
		float m_gain, m_gaindB;

		// Local parameters
		float m_thresholddB;
		float m_kneedB;
		float m_lookaheadMS;
		float m_ratio;
	};
}
