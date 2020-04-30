
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Basic impl. that got me started: https://github.com/buosseph/JuceCompressor
	Thanks to Tammo Hinrichs for a few good tips!
*/

#include "synth-compressor.h"

namespace SFM
{
	void Compressor::Apply(float *pLeft, float *pRight, unsigned numSamples)
	{
		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			// Get/set parameters
			const float peakToRMS = m_curPeakToRMS.Sample();
			/* const */ float thresholddB = m_curThresholddB.Sample();
			const float kneedB =  m_curKneedB.Sample();
			const float ratio = m_curRatio.Sample();
			const float postGain = dBToGain(m_curGaindB.Sample());
			const float lookahead = m_curLookahead.Sample();

			m_gainShaper.SetAttack(m_curAttack.Sample());
			m_gainShaper.SetRelease(m_curRelease.Sample());

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Delay input signal
			m_outDelayL.Write(sampleL);
			m_outDelayR.Write(sampleR);

			// Detect peak using non-delayed signal
			const float peakOutL  = m_detectorL.Apply(sampleL);
			const float peakOutR  = m_detectorR.Apply(sampleR);
			const float peakSum   = fast_tanhf(peakOutL+peakOutR)*0.5f; // Soft clip peak sum sounds *good*
			const float RMS       = m_RMSDetector.Run(sampleL, sampleR);
			const float sum       = lerpf<float>(peakSum, RMS, peakToRMS);
			const float sumdB     = (0.f != sum) ? GainTodB(sum) : kMinVolumedB;

			// Crush it!
			// It's fine if the knee is larger than the range on either side!
			const float halfKneedB  = kneedB*0.5f;
			thresholddB = thresholddB-halfKneedB;

			float gaindB;
			if (sumdB < thresholddB)
			{
				gaindB = 0.f;
			}
			else
			{
				// Extra assertions (in these cases we'd get divide by zero)
				SFM_ASSERT(0.f != kneedB);
				SFM_ASSERT(0.f == ratio);

				const float delta = sumdB-thresholddB;
				
				// Blend the ratio from 1 to it's destination along the knee
				const float kneeBlend = std::min<float>(delta, kneedB)/kneedB;
				const float blendRatio = lerpf<float>(1.f, ratio, kneeBlend*kneeBlend);

				gaindB = -delta * (1.f - 1.f/blendRatio);
			}

			gaindB = m_gainShaper.Apply(gaindB);
			const float gain = dBToGain(gaindB);
			
			// Apply to (delayed) signal w/gain
			
			// This is *correct*
//			const auto  delayL   = (m_outDelayL.size() - 1)*lookahead;
//			const auto  delayR   = (m_outDelayR.size() - 1)*lookahead; // Just in case R's size differs from L (for whatever reason)
//			const float delayedL = m_outDelayL.Read(delayL);
//			const float delayedR = m_outDelayR.Read(delayR);
			
			// This sounded better, test some more! (FIXME)
			const float delayedL = lerpf<float>(sampleL, m_outDelayL.ReadNearest(-1), lookahead);
			const float delayedR = lerpf<float>(sampleR, m_outDelayR.ReadNearest(-1), lookahead);

			pLeft[iSample]  = (delayedL*gain) * postGain;
			pRight[iSample] = (delayedR*gain) * postGain;	
		}
	}
}
