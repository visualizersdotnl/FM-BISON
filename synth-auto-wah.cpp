
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- Use as test for Vowelizer 2.0
		- Use BPM sync.?
*/

#include "synth-auto-wah.h"
#include "synth-distort.h"

namespace SFM
{
	constexpr double kPreLowCutQ      = 0.5;
	constexpr float  kResoRange       = 0.8f;   // Paul requested this parameter to be exposed, but I think he heard a loud popping artifact that has since been fixed
	constexpr float  kCurveOffs       = 0.1f;   // [0..1]
	constexpr float  kCutOscRange     = 0.11f;  // Frequency (normalized) / Prime number
	constexpr float  kPostCutoffOffs  = 0.05f;  //
	constexpr float  kVowelGain       = 0.707f; // -3dB (see synth-vowelizer.h)

	void AutoWah::Apply(float *pLeft, float *pRight, unsigned numSamples)
	{
		if (0.f == m_curWet.Get() && 0.f == m_curWet.GetTarget())
		{
			// This effect can introduce a pronounced delay, so if it is not in use, skip it
			// and continue without any latency

			m_curSlack.Skip(numSamples);
			m_curAttack.Skip(numSamples);
			m_curHold.Skip(numSamples);
			m_curRate.Skip(numSamples);
			m_curSpeak.Skip(numSamples);
			m_curCut.Skip(numSamples);
			m_curWet.Skip(numSamples);

			return;
		}

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
			const float clippedGain = fast_tanhf(gain);
//			const float clippedGain = gain;
			
			// Grab (delayed) signal
			
			// This is *correct*
//			const auto  delayL   = (m_outDelayL.size() - 1)*m_lookahead;
//			const auto  delayR   = (m_outDelayR.size() - 1)*m_lookahead; // Just in case R's size differs from L (for whatever reason)
//			const float delayedL = m_outDelayL.Read(delayL);
//			const float delayedR = m_outDelayR.Read(delayR);

			// This sounded better for Compressor, test some more! (FIXME)
			float delayedL = lerpf<float>(sampleL, m_outDelayL.ReadNearest(-1), m_lookahead);
			float delayedR = lerpf<float>(sampleR, m_outDelayR.ReadNearest(-1), m_lookahead);

			// Cut off high end and that's what we'll work with
			float preFilteredL = delayedL, preFilteredR = delayedR;
			m_preFilterHP.updateCoefficients(CutoffToHz(lowCut, m_Nyquist, 0.f), kPreLowCutQ, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
			m_preFilterHP.tick(preFilteredL, preFilteredR);

			// Store remainder to add back into mix
			const float remainderL = delayedL-preFilteredL;
			const float remainderR = delayedR-preFilteredR;
			
			// LFO, but only proportional to gain
			const float LFO =  m_LFO.Sample(0.f);

			float filteredL = 0.f, filteredR = 0.f;

			// Run 3 12dB low pass filters in parallel (results in a formant-like timbre, Maarten van Strien's idea)
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

					float filterL = preFilteredL, filterR = preFilteredR;
					m_preFilterLP[iPre].tick(filterL, filterR);

					filteredL += filterL;
					filteredR += filterR;
				}
				
				// Normalize
				filteredL /= 3.f;
				filteredR /= 3.f;
			}
		
			// Post filter (LP)
			const float cutRoom = kPostCutoffOffs+kCutOscRange;
			/* const */ float cutoff = cutRoom + clippedGain*(1.f-cutRoom);
			cutoff += kCutOscRange*LFO;

			const float cutHz = CutoffToHz(cutoff, m_Nyquist);
			
			// Lower signal more Q
			const float Q = ResoToQ(kResoRange - kResoRange*clippedGain);

			m_postFilterLP.updateLowpassCoeff(cutHz, Q, m_sampleRate);
			m_postFilterLP.tick(filteredL, filteredR);

			// Add (low) remainder to signal
			filteredL += remainderL;
			filteredR += remainderR;

			// Vowelize (simple "IIH-AAH")
			const float vowBlend = clippedGain;
			const float vowel_L = m_vowelizerL.Apply(filteredL*kVowelGain, Vowelizer::kI, vowBlend);
			const float vowel_R = m_vowelizerR.Apply(filteredR*kVowelGain, Vowelizer::kI, vowBlend);
			filteredL = lerpf<float>(filteredL, vowel_L, vowelize);
			filteredR = lerpf<float>(filteredR, vowel_R, vowelize);
			
			// Mix with dry (delayed) signal
			pLeft[iSample]  = lerpf<float>(delayedL, filteredL, wetness);
			pRight[iSample] = lerpf<float>(delayedR, filteredR, wetness);
		}
	}
}
