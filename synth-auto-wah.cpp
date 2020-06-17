
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME: replace lackluster "formant filter"
*/

#include "synth-auto-wah.h"
#include "synth-distort.h"

namespace SFM
{
	// Local constant parameters
	// Each of these could be a parameter but I *chose* these values; we have enough knobs as it is (thanks Stijn ;))
	constexpr double kPreLowCutQ   =    0.5;
	constexpr float  kResoMax      =   0.7f;
	constexpr float  kCutRange     =  0.05f;
	constexpr float  kVoxRateScale =    2.f;

	void AutoWah::Apply(float *pLeft, float *pRight, unsigned numSamples)
	{
		// This effect is big and expensive, we'll skip it if not used
		if (0.f == m_curWet.Get() && 0.f == m_curWet.GetTarget())
		{
			m_curResonance.Skip(numSamples);
			m_curAttack.Skip(numSamples);
			m_curHold.Skip(numSamples);
			m_curRate.Skip(numSamples);
			m_curSpeak.Skip(numSamples);
			m_curSpeakVowel.Skip(numSamples);
			m_curSpeakVowelMod.Skip(numSamples);
			m_curSpeakGhost.Skip(numSamples);
			m_curCut.Skip(numSamples);
			m_curWet.Skip(numSamples);

			for (unsigned iSample = 0; iSample  < numSamples; ++iSample)
			{
				const float sampleL = pLeft[iSample];
				const float sampleR = pRight[iSample];

				// Keep running RMS calc.
				m_RMS.Run(sampleL, sampleR);

				// Feed delay line
				m_outDelayL.Write(sampleL);
				m_outDelayR.Write(sampleR);

				// FIXME: more upkeep?
			}

			// Done
			return;
		}

		for (unsigned iSample = 0; iSample  < numSamples; ++iSample)
		{
			// Get parameters
			const float resonance = m_curResonance.Sample();
			const float curAttack = m_curAttack.Sample();
			const float curHold   = m_curHold.Sample();
			const float curRate   = m_curRate.Sample();
			const float voxWet    = m_curSpeak.Sample();
			const float voxVow    = m_curSpeakVowel.Sample();
			const float voxMod    = m_curSpeakVowelMod.Sample();
			const float voxGhost  = m_curSpeakGhost.Sample();
			const float lowCut    = m_curCut.Sample()*0.125f; // Nyquist/8 is more than enough!
			const float wetness   = m_curWet.Sample();
			
			m_sideEnv.SetAttack(curAttack *  100.f); // FIXME: why a tenth of?
			m_sideEnv.SetRelease(curHold  * 1000.f);

			m_LFO.SetFrequency(curRate);

			m_voxOscPhase.SetFrequency(curRate*kVoxRateScale);
			m_voxGhostEnv.SetRelease(kMaxWahGhostReleaseMS*0.5f + voxGhost*kMaxWahGhostReleaseMS*0.5f);

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Delay input signal
			m_outDelayL.Write(sampleL);
			m_outDelayR.Write(sampleR);

			// Calc. RMS and feed it to sidechain
			const float signaldB = m_RMS.Run(sampleL, sampleR);
			const float envdB = (signaldB > kMinVolumedB) ? m_sideEnv.Apply(signaldB) : kMinVolumedB;
			const float envGain = fast_tanhf(dB2Lin(envdB)); // Soft-clip gain, sounds good

			// Grab (delayed) signal
			const float lookahead = m_lookahead*wetness; // Lookahead is proportional to wetness, a hack to make sure we do not cause a delay when 100% dry (FIXME)
			const auto  delayL    = (m_outDelayL.size()-1)*lookahead;
			const auto  delayR    = (m_outDelayR.size()-1)*lookahead;
			const float delayedL  = m_outDelayL.Read(delayL);
			const float delayedR  = m_outDelayR.Read(delayR);

			// Cut off high end: that's what we'll work with
			float preFilteredL = delayedL, preFilteredR = delayedR;
			m_preFilterHP.updateCoefficients(CutoffToHz(lowCut, m_Nyquist, 0.f), kPreLowCutQ, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
			m_preFilterHP.tick(preFilteredL, preFilteredR);

			// Store remainder to add back into mix
			const float remainderL = delayedL-preFilteredL;
			const float remainderR = delayedR-preFilteredR;
			
			// Sample LFO
			const float LFO = m_LFO.Sample(0.f);

			// Post filter (LP)
			float filteredL = preFilteredL, filteredR = preFilteredR;

			const float cutoff = kCutRange + (envGain * (1.f - 2.f*kCutRange)) + LFO*kCutRange; // FIXME

			SFM_ASSERT(cutoff >= 0.f && cutoff <= 1.f);

			const float cutHz    = CutoffToHz(cutoff, m_Nyquist);
			const float maxNormQ = kResoMax*resonance;            //
			const float normQ    = maxNormQ - maxNormQ*envGain;   //
			const float Q        = ResoToQ(normQ);                // Less signal, more Q

			m_postFilterLP.updateLowpassCoeff(cutHz, Q, m_sampleRate);
			m_postFilterLP.tick(filteredL, filteredR);

			// Add (low) remainder to signal
			filteredL += remainderL;
			filteredR += remainderR;
			
			// Vowelize
			const float voxPhase = m_voxOscPhase.Sample();
			const float oscInput = mt_randf();
			const float voxLFO = lerpf<float>(1.f, m_voxSandH.Sample(voxPhase, oscInput, true), voxMod);
			
			// Calc. "ghost" noise
			const float ghostSig = mt_randf() * (0.3f + 0.3f*voxGhost); // This is just right for most patches
			const float ghostEnv = m_voxGhostEnv.Apply(envGain*voxLFO*voxGhost);
			const float ghost = ghostSig*ghostEnv;
			
			float vowelL = filteredL+ghost, vowelR = filteredR+ghost;
			m_vowelizerV1.Apply(vowelL, vowelR, voxVow*voxLFO);

			filteredL = lerpf<float>(filteredL, vowelL, voxWet);
			filteredR = lerpf<float>(filteredR, vowelR, voxWet);

			// Mix with dry (delayed) signal
			pLeft[iSample]  = lerpf<float>(delayedL, filteredL, wetness);
			pRight[iSample] = lerpf<float>(delayedR, filteredR, wetness);
		}
	}
}
