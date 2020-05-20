
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- Use BPM sync.
*/

#include "synth-auto-wah.h"
#include "synth-distort.h"

namespace SFM
{
	// Constant parameters
	constexpr double kPreLowCutQ  =   0.5;
	constexpr float  kPostMaxQ    =  0.7f;   
	constexpr float  kLFOCutDepth = 0.02f; // In normalized range

	void AutoWah::Apply(float *pLeft, float *pRight, unsigned numSamples)
	{
		for (unsigned iSample = 0; iSample  < numSamples; ++iSample)
		{
			// Get parameters
			const float slack     = m_curSlack.Sample(); // FIXME: unused
			const float curAttack = m_curAttack.Sample();
			const float curHold   = m_curHold.Sample();
			const float vowelize  = m_curSpeak.Sample();
			const float lowCut    = m_curCut.Sample()*0.125f; // Nyquist/8 is more than enough!
			const float wetness   = m_curWet.Sample();
			
			m_envFollower.SetAttack(curAttack*1000.f);
			m_envFollower.SetRelease(curHold*1000.f);

			m_LFO.SetFrequency(m_curRate.Sample());

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Delay input signal
			m_outDelayL.Write(sampleL);
			m_outDelayR.Write(sampleR);

			// Calc. RMS and feed it to env. follower
			const float RMS = m_RMSDetector.Run(sampleL, sampleR);
			const float signaldB = (RMS != 0.f) ? GainTodB(RMS) : kMinVolumedB;
			const float envdB = m_envFollower.Apply(signaldB, m_envdB);
			const float envGain = dBToGain(envdB);

			// Grab (delayed) signal
			const float lookahead = m_lookahead*wetness; // Lookahead is proportional to wetness, a hack to make sure we do not cause a delay when 100% dry (FIXME)
			const auto  delayL    = (m_outDelayL.size() - 1)*lookahead;
			const auto  delayR    = (m_outDelayR.size() - 1)*lookahead;
			const float delayedL  = m_outDelayL.Read(delayL);
			const float delayedR  = m_outDelayR.Read(delayR);

			// Cut off high end: that's what we'll work with
			float preFilteredL = delayedL, preFilteredR = delayedR;
			m_preFilterHP.updateCoefficients(CutoffToHz(lowCut, m_Nyquist, 0.f), kPreLowCutQ, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
			m_preFilterHP.tick(preFilteredL, preFilteredR);

			// Store remainder to add back into mix
			const float remainderL = delayedL-preFilteredL;
			const float remainderR = delayedR-preFilteredR;
			
			const float LFO = m_LFO.Sample(0.f);

			float filteredL = preFilteredL, filteredR = preFilteredR;

			// Post filter (LP)
			const float cutoff = saturatef(kLFOCutDepth + envGain*(1.f-kLFOCutDepth) + 0.5f*kLFOCutDepth*LFO); // FIXME: this doesn't look sane?
			const float cutHz  = CutoffToHz(cutoff, m_Nyquist);
			const float Q      = ResoToQ(kPostMaxQ - kPostMaxQ*envGain); // Lower signal, more Q

			m_postFilterLP.updateLowpassCoeff(cutHz, Q, m_sampleRate);
			m_postFilterLP.tick(filteredL, filteredR);

			// Add (low) remainder to signal
			filteredL += remainderL;
			filteredR += remainderR;

			// Vowelize (V2)
			const VowelizerV2::Vowel vowelA = VowelizerV2::kOO;
			const VowelizerV2::Vowel vowelB = VowelizerV2::kA;
			const float vowBlend = envGain;

			float vowelL_1 = filteredL, vowelR_1 = filteredR;
			m_vowelizerV2_1.Apply(vowelL_1, vowelR_1, vowelA);

			float vowelL_2 = filteredL, vowelR_2 = filteredR;
			m_vowelizerV2_1.Apply(vowelL_2, vowelR_2, vowelB);
			
			const float vowelL = lerpf<float>(vowelL_1, vowelL_2, vowBlend);
			const float vowelR = lerpf<float>(vowelR_1, vowelR_2, vowBlend);

			filteredL = lerpf<float>(filteredL, vowelL, vowelize);
			filteredR = lerpf<float>(filteredR, vowelR, vowelize);

			// Vowelize (legacy)
//			const float vowBlend = kLFOVowDepth + envGain*(1.f-kLFOVowDepth) + 0.5f*kLFOVowDepth*LFO;
//			const float vowelL = m_vowelizerL.Apply(filteredL*0.707f, Vowelizer::kU, vowBlend);
//			const float vowelR = m_vowelizerR.Apply(filteredR*0.707f, Vowelizer::kU, vowBlend);
//			filteredL = lerpf<float>(filteredL, vowelL, vowelize);
//			filteredR = lerpf<float>(filteredR, vowelR, vowelize);

			// Mix with dry (delayed) signal
			pLeft[iSample]  = lerpf<float>(delayedL, filteredL, wetness);
			pRight[iSample] = lerpf<float>(delayedR, filteredR, wetness);
		}
	}
}
