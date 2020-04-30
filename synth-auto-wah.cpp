
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- I've corrected for the low cut a bit so the effect does not diminish too fast, but it's late, and it can probably be done better
		- Move hardcoded values to constants on top
		- Use BPM sync.?
*/

#include "synth-auto-wah.h"

namespace SFM
{
	void AutoWah::Apply(float *pLeft, float *pRight, unsigned numSamples)
	{
		for (unsigned iSample = 0; iSample  < numSamples; ++iSample)
		{
			// Update parameters
			const float slack = m_curSlack.Sample();
			m_detectorL.SetRelease(slack);
			m_detectorR.SetRelease(slack);

			m_gainShaper.SetAttack(m_curSpeed.Sample());
			m_gainShaper.SetRelease(m_curHold.Sample());

			m_LFO.SetFrequency(m_curRate.Sample());

			const float vowelize  = m_curSpeak.Sample();
			const float lowCut    = m_curCut.Sample()*0.125f; // Nyquist/8 is more than enough!
			const float wetness   = m_curWet.Sample();

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
			const float sum       = peakSum;
			const float sumdB     = (0.f != sum) ? GainTodB(sum) : kMinVolumedB;

			// Calculate gain
			const float gaindB      = m_gainShaper.Apply(sumdB);
			const float gain        = dBToGain(gaindB);
			const float clippedGain = fast_tanhf(gain);
			
			// Grab (delayed) signal
			
			// This is *correct*
//			const auto  delayL   = (m_outDelayL.size() - 1)*m_lookahead;
//			const auto  delayR   = (m_outDelayR.size() - 1)*m_lookahead; // Just in case R's size differs from L (for whatever reason)
//			const float delayedL = m_outDelayL.Read(delayL);
//			const float delayedR = m_outDelayR.Read(delayR);

			// This sounded better for Compressor, test some more! (FIXME)
			float delayedL = lerpf<float>(sampleL,  m_outDelayL.ReadNearest(-1), m_lookahead);
			float delayedR = lerpf<float>(sampleR, m_outDelayR.ReadNearest(-1), m_lookahead);

			// Cut off high end and that's what we'll work with
			float preFilteredL = delayedL, preFilteredR = delayedR;
			m_preFilterHP.updateCoefficients(CutoffToHz(lowCut, m_Nyquist, 0.f), 0.5, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
			m_preFilterHP.tick(preFilteredL, preFilteredR);

			// Store remainder to add back into mix
			const float remainderL = delayedL-preFilteredL;
			const float remainderR = delayedR-preFilteredR;

			// Replace
			delayedL = preFilteredL;
			delayedR = preFilteredR;

			float filteredL = 0.f, filteredR = 0.f;

			// Run 3 12dB low pass filters in parallel (results in a formant-like timbre)
			{
				float curCutoff = 0.1f + lowCut;
				const float Q = ResoToQ(0.5f - clippedGain*0.5f);
				
				// Expand
				const float spread = (0.3f - lowCut/3.f)*(0.01f + (clippedGain-0.01f));

				for (unsigned iPre = 0; iPre < 3; ++iPre)
				{
					const float cutHz = CutoffToHz(curCutoff, m_Nyquist);
					m_preFilterLP[iPre].updateLowpassCoeff(cutHz, Q, m_sampleRate);

					float filterL = delayedL, filterR = delayedR;
					m_preFilterLP[iPre].tick(filterL, filterR);

					filteredL += filterL;
					filteredR += filterR;

					curCutoff += spread;
				}
				
				// Normalize
				filteredL /= 3.f;
				filteredR /= 3.f;
			}
			
			// Post filter (LP)
			const float LFO = 0.09f*m_LFO.Sample(0.f);
			const float cutoff = std::max<float>(0.f, clippedGain*0.9f + LFO);
			const float cutHz = CutoffToHz(cutoff, m_Nyquist);
			const float Q = ResoToQ(0.9f - (0.15f*wetness + clippedGain*0.65f));
			m_postFilterLP.updateLowpassCoeff(cutHz, Q, m_sampleRate);
			m_postFilterLP.tick(filteredL, filteredR);

			// Add (low) remainder to signal
			const float postFilterL = remainderL+filteredL;
			const float postFilterR = remainderR+filteredR;

			// Vowelize ("AAH" -> "UUH")
			const float vowel_L = m_vowelizerL.Apply(postFilterL*0.5f, Vowelizer::kA, clippedGain);
			const float vowel_R = m_vowelizerR.Apply(postFilterR*0.5f, Vowelizer::kA, clippedGain);
			
			// Blend to final filtered mix
			const float finalL = lerpf<float>(postFilterL, vowel_L, vowelize);
			const float finalR = lerpf<float>(postFilterR, vowel_R, vowelize);

			// Mix with dry signal
			pLeft[iSample]  = lerpf<float>(sampleL, finalL, wetness);
			pRight[iSample] = lerpf<float>(sampleR, finalR, wetness);
		}
	}
}
