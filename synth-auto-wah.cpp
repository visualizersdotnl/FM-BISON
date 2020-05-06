
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- Use as test for Vowelizer 2.0
		- Use BPM sync.
*/

#include "synth-auto-wah.h"
#include "synth-distort.h"

namespace SFM
{
	constexpr double kPreLowCutQ      = 0.5;
	constexpr float  kPostMaxQ        = 0.8f;   // Paul requested this parameter to be exposed, but I think he heard a loud popping artifact that has since been fixed
	constexpr float  kVowelGain       = 0.707f; // -3dB (see synth-vowelizer.h)
	constexpr float  kLFODepth        = 0.01f;  // In normalized freq.

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
			const float gaindB = m_gainShaper.Apply(sumdB);
			const float gain   = dBToGain(gaindB);
			
			// Grab (delayed) signal
			
			// This is *correct*
//			const auto  delayL   = (m_outDelayL.size() - 1)*m_lookahead;
//			const auto  delayR   = (m_outDelayR.size() - 1)*m_lookahead; // Just in case R's size differs from L (for whatever reason)
//			const float delayedL = m_outDelayL.Read(delayL);
//			const float delayedR = m_outDelayR.Read(delayR);

			// This sounded better for Compressor, though here we factor in wetness so the signal
			// ain't delayed when we're running dry, which is 100% nonsense pseudo-logic (FIXME)
			const float lookahead = m_lookahead*wetness;
			float delayedL = lerpf<float>(sampleL, m_outDelayL.ReadNearest(-1), m_lookahead);
			float delayedR = lerpf<float>(sampleR, m_outDelayR.ReadNearest(-1), m_lookahead);

			// Cut off high end and that's what we'll work with
			float preFilteredL = delayedL, preFilteredR = delayedR;
			m_preFilterHP.updateCoefficients(CutoffToHz(lowCut, m_Nyquist, 0.f), kPreLowCutQ, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
			m_preFilterHP.tick(preFilteredL, preFilteredR);

			// Store remainder to add back into mix
			const float remainderL = delayedL-preFilteredL;
			const float remainderR = delayedR-preFilteredR;
			
			// LFO (shall I proportion this to gain?)
			const float LFO =  m_LFO.Sample(0.f);

			float filteredL = preFilteredL, filteredR = preFilteredR;

			// Vowelize (using legacy Vowelizer)
			const float vowBlend = gain;
			const float vowelL = m_vowelizerL.Apply(filteredL*kVowelGain, Vowelizer::kI, vowBlend);
			const float vowelR = m_vowelizerR.Apply(filteredR*kVowelGain, Vowelizer::kI, vowBlend);
			filteredL = lerpf<float>(filteredL, vowelL, vowelize);
			filteredR = lerpf<float>(filteredR, vowelR, vowelize);

			// Post filter (LP)
			const float cutoff = kLFODepth + gain*(1.f-kLFODepth) + 0.5f*kLFODepth*LFO;
			const float cutHz = CutoffToHz(cutoff, m_Nyquist);
			
			// Lower signal, more Q
			const float Q = ResoToQ(kPostMaxQ - kPostMaxQ*gain);

			m_postFilterLP.updateLowpassCoeff(cutHz, Q, m_sampleRate);
			m_postFilterLP.tick(filteredL, filteredR);

			// Add (low) remainder to signal
			filteredL += remainderL;
			filteredR += remainderR;

//			float vowelL = filteredL, vowelR = filteredR;
//			m_vowelizerV2.Apply(vowelL, vowelR, VowelizerV2::kEE);
//			filteredL = lerpf<float>(filteredL, vowelL, vowelize);
//			filteredR = lerpf<float>(filteredR, vowelR, vowelize);

			// Mix with dry (delayed) signal
			pLeft[iSample]  = lerpf<float>(delayedL, filteredL, wetness);
			pRight[iSample] = lerpf<float>(delayedR, filteredR, wetness);
		}
	}
}
