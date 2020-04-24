
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Basic impl. that got me started: https://github.com/buosseph/JuceCompressor
	Thanks to Tammo Hinrichs for a few good tips!
*/

#pragma once

// #include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"
#include "synth-followers.h"
#include "synth-delay-line.h"

namespace SFM
{
	class Compressor
	{
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
			float m_attack;
			float m_release;
			
			float m_outGain;
			double m_attackB0, m_releaseB0;
		};

	public:
		// FIXME?
		const float kCompDelay = 0.01f; 

		Compressor(unsigned sampleRate) :
			m_sampleRate(sampleRate)
,			m_outDelayL(sampleRate, kCompDelay)
,			m_outDelayR(sampleRate, kCompDelay)
,			m_detectorL(sampleRate)
,			m_detectorR(sampleRate)
,			m_RMSDetector(sampleRate, 0.005f /* 5MS: Reaper's compressor default */)
,			m_gainShaper(sampleRate, kDefCompAttack, kDefCompRelease)
		{
 			SetParameters(kDefCompPeakToRMS, kDefCompThresholddB, kDefCompKneedB, kDefCompRatio, kDefCompGaindB, kDefCompAttack, kDefCompRelease, 0.f);
		}

		~Compressor() {}

		SFM_INLINE void SetParameters(float peakToRMS, float thresholddB, float kneedB, float ratio, float gaindB, float attack, float release, float lookahead)
		{
			SFM_ASSERT(peakToRMS >= 0.f && peakToRMS <= 1.f);
			SFM_ASSERT(thresholddB >= kMinCompThresholdB && thresholddB <= kMaxCompThresholdB);
			SFM_ASSERT(kneedB >= kMinCompKneedB && kneedB <= kMaxCompKneedB);
			SFM_ASSERT(ratio >= kMinCompRatio && ratio <= kMaxCompRatio);
			SFM_ASSERT(gaindB >= kMinCompGaindB && gaindB <= kMaxCompGaindB);
			SFM_ASSERT(attack >= kMinCompAttack && attack <= kMaxCompAttack);
			SFM_ASSERT(release >= kMinCompRelease && attack <= kMaxCompRelease);
			SFM_ASSERT(lookahead >= 0.f && lookahead <= 0.f);

			m_peakToRMS = peakToRMS;
			m_thresholddB = thresholddB;
			m_kneedB = kneedB;
			m_ratio = ratio;
			m_postGain = dBToGain(gaindB);

			m_gainShaper.SetAttack(attack);
			m_gainShaper.SetRelease(release);

			m_lookahead = lookahead;
		}

		SFM_INLINE void Apply(float &left, float &right)
		{
			// Delay input signal
			m_outDelayL.Write(left);
			m_outDelayR.Write(right);

			// Detect peak using non-delayed signal
			const float peakOutL  = m_detectorL.Apply(left);
			const float peakOutR  = m_detectorR.Apply(right);
			const float peakSum   = fast_tanhf(peakOutL+peakOutR)*0.5f; // Soft clip peak sum sounds *good*
			const float RMS       = m_RMSDetector.Run(left, right);
			const float sum       = lerpf<float>(peakSum, RMS, m_peakToRMS);
			const float sumdB     = (0.f != sum) ? GainTodB(sum) : kMinVolumedB;

			// Crush it!
			const float halfKneedB  = m_kneedB*0.5f;
			const float thresholddB = m_thresholddB-halfKneedB;

			float gaindB;
			if (sumdB < thresholddB)
			{
				gaindB = 0.f;
			}
			else
			{
				// Extra assertions (in these cases we'd get divide by zero)
				SFM_ASSERT(0.f != m_kneedB);
				SFM_ASSERT(0.f == m_ratio);

				const float delta = sumdB-thresholddB;
				
				// Blend the ratio from 1 to it's destination along the knee
				const float kneeBlend = std::min<float>(delta, m_kneedB)/m_kneedB;
				const float ratio = lerpf<float>(1.f, m_ratio, kneeBlend*kneeBlend);

				gaindB = -delta * (1.f - 1.f/ratio);
			}

			gaindB = m_gainShaper.Apply(gaindB);
			const float gain = dBToGain(gaindB);
			
			// Apply to (delayed) signal w/gain
			// FIXME: it is not correct to interpolate like this, but it sounded better than adjusting the actual delay
			const float delayedL = lerpf<float>(left,  m_outDelayL.ReadNearest(-1), m_lookahead);
			const float delayedR = lerpf<float>(right, m_outDelayR.ReadNearest(-1), m_lookahead);
			left  = (delayedL*gain) * m_postGain;
			right = (delayedR*gain) * m_postGain;
		}

	private:
		const unsigned m_sampleRate;

		DelayLine m_outDelayL, m_outDelayR;
		PeakDetector m_detectorL, m_detectorR;
		RMSDetector m_RMSDetector;
		GainShaper m_gainShaper;

		// Parameters
		float m_peakToRMS;
		float m_thresholddB;
		float m_kneedB;
		float m_ratio;
		float m_postGain;
		float m_lookahead;
	};
}
