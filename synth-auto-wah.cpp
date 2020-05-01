
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- Define constant values used in Apply() on top
		- Use as test for Vowelizer 2.0
		- Use BPM sync.?
*/

#include "synth-auto-wah.h"

namespace SFM
{
	const double kPreLowCutQ   = 0.5;
	const float  kResoRange    = 0.9f;
	const float  kCurveOffs    = 0.1f;  // [0..1]
	const float  kCutOscRange  = 0.09f; // Keep it small!

	void AutoWah::Apply(float *pLeft, float *pRight, unsigned numSamples)
	{
		for (unsigned iSample = 0; iSample  < numSamples; ++iSample)
		{
			// Get/set parameters
			const float slack = m_curSlack.Sample();
			m_detectorL.SetRelease(slack);
			m_detectorR.SetRelease(slack);

			m_gainShaper.SetAttack(m_curAttack.Sample());
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
//			const float clippedGain = fast_tanhf(gain);
			const float clippedGain = gain;
			
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
			m_preFilterHP.updateCoefficients(CutoffToHz(lowCut, m_Nyquist, 0.f), kPreLowCutQ, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
			m_preFilterHP.tick(preFilteredL, preFilteredR);

			// Store remainder to add back into mix
			const float remainderL = delayedL-preFilteredL;
			const float remainderR = delayedR-preFilteredR;

			// Replace
			delayedL = preFilteredL;
			delayedR = preFilteredR;

			const float LFO = m_LFO.Sample(0.f);

			float filteredL = 0.f, filteredR = 0.f;

			// Run 3 12dB low pass filters in parallel (results in a formant-like timbre)
			{
				float curX = kCurveOffs + lowCut; // 'lowCut' adds an offset
				const float delta = (1.f-curX)/3.f;

				for (unsigned iPre = 0; iPre < 3; ++iPre)
				{
					const float curCutoff = expf(curX*kPI*0.1f) - 1.f; // Tame slope that should *not* exceed 0.5f within [0..1]
					SFM_ASSERT(curCutoff <= 0.5f);
					curX += delta;
					
					// More cutoff means less Q
					const float Q = ResoToQ(0.5f - curCutoff);

					const float cutHz = CutoffToHz(curCutoff, m_Nyquist);
					m_preFilterLP[iPre].updateLowpassCoeff(cutHz, Q, m_sampleRate);

					float filterL = delayedL, filterR = delayedR;
					m_preFilterLP[iPre].tick(filterL, filterR);

					filteredL += filterL;
					filteredR += filterR;
				}
				
				// Normalize
				filteredL /= 3.f;
				filteredR /= 3.f;
			}
		
			// Post filter (LP)
			const float base = 1.f - kCutOscRange*2.f;                               // Reserve twice the range so the LFO never closes the filter
			const float cutoff = kCutOscRange + clippedGain*base + kCutOscRange*LFO; //
			const float cutHz = CutoffToHz(cutoff, m_Nyquist);
			
			// Lower signal more Q
			const float Q = ResoToQ(kResoRange - kResoRange*clippedGain);

			m_postFilterLP.updateLowpassCoeff(cutHz, Q, m_sampleRate);
			m_postFilterLP.tick(filteredL, filteredR);

			// Add (low) remainder to signal
			filteredL += remainderL;
			filteredR += remainderR;

			// Vowelize ("AAH")
			const float vowel_L = m_vowelizerL.Apply(filteredL*0.5f, Vowelizer::kA, clippedGain);
			const float vowel_R = m_vowelizerR.Apply(filteredR*0.5f, Vowelizer::kA, clippedGain);

			// Mix vowel
			filteredL = lerpf<float>(filteredL, vowel_L, vowelize);
			filteredR = lerpf<float>(filteredR, vowel_R, vowelize);
			
			// Mix with dry signal
			pLeft[iSample]  = lerpf<float>(sampleL, filteredL, wetness);
			pRight[iSample] = lerpf<float>(sampleR, filteredR, wetness);
		}
	}
}
