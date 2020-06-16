
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
	// Constant parameter(s)
	// Each of these could be a parameter but I *chose* these values (thank you, Stijn)
	constexpr double kPreLowCutQ  =    0.5;
	constexpr float  kResoMax     =   0.7f;
	constexpr float  kCutRange    =  0.05f;

	void AutoWah::Apply(float *pLeft, float *pRight, unsigned numSamples)
	{
		constexpr float kGain3dB = 1.41253757f;

		if (0.f == m_curWet.Get() && 0.f == m_curWet.GetTarget())
		{
			m_curResonance.Skip(numSamples);
			m_curAttack.Skip(numSamples);
			m_curHold.Skip(numSamples);
			m_curSpeak.Skip(numSamples);
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
			const float vowelize  = m_curSpeak.Sample();
			const float lowCut    = m_curCut.Sample()*0.125f; // Nyquist/8 is more than enough!
			const float wetness   = m_curWet.Sample();
			
			m_sideEnv.SetAttack(curAttack *  100.f); // FIXME: why a tenth of?
			m_sideEnv.SetRelease(curHold  * 1000.f);

			m_LFO.SetFrequency(m_curRate.Sample());

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Delay input signal
			m_outDelayL.Write(sampleL);
			m_outDelayR.Write(sampleR);

			// Calc. RMS and feed it to sidechain
			const float signaldB = m_RMS.Run(sampleL, sampleR);
			const float envdB = m_sideEnv.Apply(signaldB);
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
			
			const float LFO = m_LFO.Sample(0.f);

			float filteredL = preFilteredL, filteredR = preFilteredR;

			// Post filter (LP)
			const float cutoff = kCutRange + (envGain * (1.f - 2.f*kCutRange)) + LFO*kCutRange;

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

			// "Vowelize" (until I have a better formant filter)
			const Vowel vowelA = Vowel::kA;
			const Vowel vowelB = Vowel::kEE;
			const float vowBlend = envGain;

			float vowelL_1 = filteredL, vowelR_1 = filteredR;
			m_vowelizerV2_1.Apply(vowelL_1, vowelR_1, vowelA);

			float vowelL_2 = filteredL, vowelR_2 = filteredR;
			m_vowelizerV2_1.Apply(vowelL_2, vowelR_2, vowelB);
			
			float vowelL = lerpf<float>(vowelL_1, vowelL_2, vowBlend);
			float vowelR = lerpf<float>(vowelR_1, vowelR_2, vowBlend);

			filteredL = lerpf<float>(filteredL, vowelL, vowelize);
			filteredR = lerpf<float>(filteredR, vowelR, vowelize);

#if 0
			// Formant pass
			float formCarrierL = filteredL, formCarrierR = filteredR;
			m_formantFilter.Apply(formCarrierL, formCarrierR, vowelL, vowelR);

			filteredL = lerpf<float>(filteredL, formCarrierL, vowelize);
			filteredR = lerpf<float>(filteredR, formCarrierR, vowelize);
#endif

			// Mix with dry (delayed) signal
			pLeft[iSample]  = lerpf<float>(delayedL, filteredL, wetness);
			pRight[iSample] = lerpf<float>(delayedR, filteredR, wetness);
		}
	}
}
