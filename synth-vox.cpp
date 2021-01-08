
/*
	FM. BISON hybrid FM synthesis -- Vowelizer ('vox') effect implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-vox.h"
#include "synth-DX7-LFO-table.h" // FIXME: necessary?

namespace SFM
{
	// Local constant parameters (I've got enough paramaters as it is!)
	constexpr float kPreLowCutQ    = kSVFMinFilterQ; // Q (SVF range)
	constexpr float kLPResoMin     = 0.01f;          // Q (normalized)
	constexpr float kLPResoMax     =  0.6f;          //
	constexpr float kLPCutLFORange = 0.99f;          // LFO cutoff range (normalized)

	constexpr float kVoxRateScale  =   2.f; // Rate ratio: vox. S&H
	constexpr float kCutRateScale  = 0.25f; // Rate ratio: cutoff modulation
	
	void Vox::Apply(float *pLeft, float *pRight, unsigned numSamples, bool manualRate)
	{
		// FIXME: VowelizerV1 only supports a fixed sample rate, so we'll just skip the nearest amount of samples
		//        so it will sound nearly the same at different sample rates; must be replaced by my own vocoder soon!

		const unsigned voxSampleRate = m_vowelizerV1.GetSampleRate();
		const float voxRatio = std::max<float>(1.f, m_sampleRate/float(voxSampleRate));
		const unsigned voxIntRatio = unsigned(roundf(voxRatio)); // Simply round to closest integer

		float vowelHoldL = 0.f, vowelHoldR = 0.f;

		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			// Sample parameters
			const float resonance  = m_curResonance.Sample();
			const float curAttack  = m_curAttack.Sample();
			const float curHold    = m_curHold.Sample();
			const float curRate    = m_curRate.Sample();
			const float curSensLin = dB2Lin(m_curDrivedB.Sample());
			const float voxWet     = m_curSpeak.Sample();
			const float voxVow     = m_curSpeakVowel.Sample();
			const float voxMod     = m_curSpeakVowelMod.Sample();
			const float voxGhost   = m_curSpeakGhost.Sample();
			const float voxCut     = m_curSpeakCut.Sample();
			const float voxReso    = m_curSpeakReso.Sample();
			const float lowCut     = m_curCut.Sample()*0.125f; // Nyquist/8 is more than enough!
			const float wetness    = m_curWet.Sample();
			
			// Set parameters
			m_gainEnvdB.SetAttack(curAttack*100.f); // FIXME: why does this *sound* right at one tenth of what it should be?
			m_gainEnvdB.SetRelease(curHold*100.f);  // 

			// BPM sync. or manual?
			const float adjRate = (true == manualRate)
				? MIDI_To_DX7_LFO_Hz(curRate) // Fetch rate in Hz from DX7 LFO table
				: 2.f/curRate;                // This ratio works, tested with a metronome and delay effect with feedback enabled

			m_LFO.SetFrequency(adjRate*kCutRateScale);

			m_voxOscPhase.SetFrequency(adjRate*kVoxRateScale);
			m_voxGhostEnv.SetRelease(kMinWahGhostReleaseMS + voxGhost*(kMaxWahGhostReleaseMS-kMinWahGhostReleaseMS));

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Calc. env. gain
//			const float signaldB  = m_RMS.Run(sampleL, sampleR);
			const float signaldB  = m_peak.Run(sampleL, sampleR);
			const float envGaindB = m_gainEnvdB.Apply(signaldB);
			const float envGain   = dB2Lin(envGaindB);

			if (envGain <= kEpsilon)
			{
				// It feels right to reset the "motion" during silence
				m_LFO.Reset();
			}

			// Cut off high end: that's what we'll work with
			float preFilteredL = sampleL, preFilteredR = sampleR;
			m_preFilterHPF.updateCoefficients(SVF_CutoffToHz(lowCut, m_Nyquist), kPreLowCutQ, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
			m_preFilterHPF.tick(preFilteredL, preFilteredR);

			// Store remainder to add back into mix
			const float remainderL = sampleL-preFilteredL;
			const float remainderR = sampleR-preFilteredR;
			
			// Gain adjusted by signal drive
			const float sensEnvGain = std::fminf(1.f, envGain*curSensLin);

			/*
				Post filter (LPF)
			*/

			float filteredL = preFilteredL, filteredR = preFilteredR;
			
			// Calc. cutoff
			const float LFO = m_LFO.Sample(0.f);
			const float modLFO = fabsf(LFO)*sensEnvGain;
			const float normCutoff = (1.f-kLPCutLFORange) + modLFO*kLPCutLFORange;

			SFM_ASSERT(normCutoff >= 0.f && normCutoff <= 1.f);
			const float cutoffHz = SVF_CutoffToHz(normCutoff, m_Nyquist);

			// Calc. Q (less signal more resonance, gives the sweep a nice bite)
			const float rangeQ = resonance*(kLPResoMax-kLPResoMin);            
			const float normQ  = kLPResoMin + rangeQ*(1.f-sensEnvGain);
			const float Q      = SVF_ResoToQ(normQ);             

			m_postFilterLPF.updateLowpassCoeff(cutoffHz, Q, m_sampleRate);
			m_postFilterLPF.tick(filteredL, filteredR);

			/*
				Add (low) remainder to signal
			*/

			filteredL += remainderL;
			filteredR += remainderR;

			/*
				Vowelize

				FIXME: until VowelizerV1 is replaced, everything is calculated as usual *except* that VowelizerV1 is sampled at it's
				       own sample rate as it's coefficients were generated for it specifically; otherwise the pitch changes drastically as the main sample rate goes up
			*/

			// Calc. vox. LFO A (sample) and B (amplitude)
			const float voxPhase  = m_voxOscPhase.Sample();
			const float oscInput  = mt_randfc()*0.995f; // Evade edges
			const float voxOsc    = m_voxSandH.Sample(voxPhase, oscInput);
			const float toLFO     = steepstepf(voxMod);
			const float voxLFO_A  = lerpf<float>(0.f, voxOsc, toLFO);
			const float voxLFO_B  = lerpf<float>(1.f, fabsf(voxOsc), toLFO);
			
			// Calc. vox. "ghost" noise
			const float ghostRand = mt_randfc();
			const float ghostSig  = ghostRand;
			const float ghostEnv  = m_voxGhostEnv.Apply(sensEnvGain * voxLFO_B * voxGhost);
			const float ghost     = ghostSig*ghostEnv;

			// Calc. vowel
			static_assert(1 + unsigned(kMaxWahSpeakVowel) < VowelizerV1::kNumVowels-1);
			const float vowel = (1.f+voxVow) + voxLFO_A;
		
			// Filter and mix
			float vowelL = filteredL + ghost, vowelR = filteredR + ghost;

			// Apply LPF
			m_voxLPF.updateLowpassCoeff(SVF_CutoffToHz(voxCut, m_Nyquist), SVF_ResoToQ(voxReso), m_sampleRate);
			m_voxLPF.tick(vowelL, vowelR);
			
			// Sample
			if (0 == (iSample % voxIntRatio))
			{
				// Apply filter (introduces a few harmonics in the top end)
				m_vowelizerV1.Apply(vowelL, vowelR, vowel);
			
				// Hold values until next tick
				vowelHoldL = vowelL;
				vowelHoldR = vowelR;
			}

			filteredL = lerpf<float>(filteredL, vowelHoldL, voxWet); // Mix with 'held' values
			filteredR = lerpf<float>(filteredR, vowelHoldR, voxWet); //

			if (GetRectifiedMaximum(filteredL, filteredR) <= kEpsilon)
			{
				// Reset filters (FIXME: hack to stabilize continuous SVF filter w/o oversampling)
				m_preFilterHPF.resetState();
				m_postFilterLPF.resetState();
				m_voxLPF.resetState();
			}

			/*
				Final mix
			*/

			pLeft[iSample]  = lerpf<float>(sampleL, filteredL, wetness);
			pRight[iSample] = lerpf<float>(sampleR, filteredR, wetness);
		}
	}
}
