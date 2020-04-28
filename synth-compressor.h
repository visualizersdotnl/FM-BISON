
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Basic impl. that got me started: https://github.com/buosseph/JuceCompressor
	Thanks to Tammo Hinrichs for a few good tips!
*/

#pragma once

#include "synth-global.h"
#include "synth-followers.h"
#include "synth-delay-line.h"

namespace SFM
{
	class Compressor
	{
	public:
		// FIXME: parameter?
		const float kCompMaxDelay = 0.01f; 

		Compressor(unsigned sampleRate) :
			m_sampleRate(sampleRate)
,			m_outDelayL(sampleRate, kCompMaxDelay)
,			m_outDelayR(sampleRate, kCompMaxDelay)
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
			// I don't feel it's a problem if the knee is larger than the range on either side
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
			
			// This is *correct*
//			const auto  delayL   = (m_outDelayL.size() - 1)*m_lookahead;
//			const auto  delayR   = (m_outDelayR.size() - 1)*m_lookahead; // Just in case R's size differs from L (for whatever reason)
//			const float delayedL = m_outDelayL.Read(delayL);
//			const float delayedR = m_outDelayR.Read(delayR);
			
			// This sounded better, test some more! (FIXME)
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
