
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-auto-wah.h"
#include "synth-DX7-LFO-table.h"

namespace SFM
{
	// Local constant parameters (I've got enough paramaters as it is!)
	constexpr double kPreLowCutQ    =   0.5; // Q (SVF range)
	constexpr float  kLPResoMin     = 0.01f; // Q (normalized)
	constexpr float  kLPResoMax     =  0.6f; //
	constexpr float  kLPCutLFORange = 0.95f; // LFO cutoff range (normalized)

	constexpr float  kVoxRateScale  =   2.f; // Rate ratio: vox. S&H
	constexpr float  kCutRateScale  = 0.25f; // Rate ratio: cutoff modulation
	
	// dBs
//	constexpr float kVoxGhostNoiseGain = 0.35481338923357547f; // -9dB
//	constexpr float kVoxGhostNoiseGain = 0.5f; // -6dB
	constexpr float kVoxGhostNoiseGain = 1.f;  // -0dB
	constexpr float kGain3dB = 1.41253757f;
	constexpr float kGainInf = kEpsilon; // FIXME: use dB2Lin(kInfVolumedB)
	
	void AutoWah::Apply(float *pLeft, float *pRight, unsigned numSamples, bool manualRate)
	{
		// This effect is big and expensive, we'll skip it if not used
		if (0.f == m_curWet.Get() && 0.f == m_curWet.GetTarget())
		{
			m_curResonance.Skip(numSamples);
			m_curAttack.Skip(numSamples);
			m_curHold.Skip(numSamples);
			m_curRate.Skip(numSamples);
			m_curDrivedB.Skip(numSamples);
			m_curResonance.Skip(numSamples);
			m_curSpeak.Skip(numSamples);
			m_curSpeakVowel.Skip(numSamples);
			m_curSpeakVowelMod.Skip(numSamples);
			m_curSpeakGhost.Skip(numSamples);
			m_curSpeakCut.Skip(numSamples);
			m_curSpeakReso.Skip(numSamples);
			m_curCut.Skip(numSamples);
			m_curWet.Skip(numSamples);

			for (unsigned iSample = 0; iSample  < numSamples; ++iSample)
			{
				const float sampleL = pLeft[iSample];
				const float sampleR = pRight[iSample];

				// Keep running peak calc.
				m_peak.Run(sampleL, sampleR);
			}

			// Done
			return;
		}
		
		// FIXME: VowelizerV1 has coefficients for a fixed sample rate, so we'll be simply skipping those few samples
		//        Not the best sounding nor most elegant solution, but I'm eagerly waiting to replace it for my own
		//        (vowel) vocoder soon (FIXME)

		const unsigned voxSampleRate = m_vowelizerV1.GetSampleRate();
		const float voxRatio = std::max<float>(1.f, m_sampleRate/float(voxSampleRate));
		const unsigned voxIntRatio = unsigned(roundf(voxRatio)); // Just round to the closest integer, fast and easy

		// Borrow this class to linearly interpolate between results (should be fine with so little samples)
		InterpolatedParameter<kLinInterpolate> InterpolatedVowelL(0.f, voxIntRatio), InterpolatedVowelR(0.f, voxIntRatio);

		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			// Get parameters
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

			// Pick rate from DX7 table if not in BPM sync. mode
			const float adjRate = (true == manualRate) ? MIDI_To_DX7_LFO_Hz(curRate) : curRate;

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

			if (envGain <= kGainInf) // Sidechain dB below or equal to defined minimum?
			{
				// Reset LFO
				m_LFO.Reset();

				// Reset 'Vox' objects
				m_voxOscPhase.Reset(); // S&H phase
				m_voxSandH.Reset();    // S&H state
//				m_voxGhostEnv.Reset(); // Ghost follower
//				m_voxLPF.resetState(); // Reset SVF
//				m_vowelizerV1.Reset(); // Reset coefficients
			}

			// Cut off high end: that's what we'll work with
			float preFilteredL = sampleL, preFilteredR = sampleR;
			m_preFilterHP.updateCoefficients(CutoffToHz(lowCut, m_Nyquist), kPreLowCutQ, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
			m_preFilterHP.tick(preFilteredL, preFilteredR);

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
			const float cutoffHz = CutoffToHz(normCutoff, m_Nyquist);

			// Calc. Q (less signal more resonance, gives the sweep a nice bite)
			const float rangeQ = resonance*(kLPResoMax-kLPResoMin);            
			const float normQ  = kLPResoMin + rangeQ*(1.f-sensEnvGain);
			const float Q      = ResoToQ(normQ);             

			m_postFilterLP.updateLowpassCoeff(cutoffHz, Q, m_sampleRate);
			m_postFilterLP.tick(filteredL, filteredR);

			/*
				Add (low) remainder to signal
			*/

			filteredL += remainderL;
			filteredR += remainderR;

			/*
				Vowelize

				FIXME: until VowelizerV1 is replaced everything is calculated as usual, except for that little block below that only samples
				       every N samples and interpolates in between because it can't handle arbitrary sample rates
			*/

			// Calc. vox. LFO A (sample) and B (amplitude)
			const double voxPhase = m_voxOscPhase.Sample();
			const float oscInput  = mt_randfc();
			const float voxOsc    = m_voxSandH.Sample(voxPhase, oscInput);
			const float toLFO     = steepstepf(voxMod);
			const float voxLFO_A  = lerpf<float>(0.f, voxOsc, toLFO);
			const float voxLFO_B  = lerpf<float>(1.f, fabsf(voxOsc), toLFO);
			
			// Calc. vox. "ghost" noise
			const float ghostRand = mt_randf();
			const float ghostSig  = ghostRand*kVoxGhostNoiseGain;
			const float ghostEnv  = m_voxGhostEnv.Apply(sensEnvGain * voxLFO_B * voxGhost);
			const float ghost     = ghostSig*ghostEnv;

			// Calc. vowel
			// I dislike frequent fmodf() calls but according to MSVC's profiler we're in the clear
			// I add a small amount to the maximum since we need to actually reach kMaxWahSpeakVowel
			static_assert(unsigned(kMaxWahSpeakVowel) < VowelizerV1::kNumVowels-1);
			const float vowel = fabsf(fmodf(voxVow+voxLFO_A, kMaxWahSpeakVowel + 0.001f /* Leaks into 'U' vowel, which is quite similar */));
		
			// Filter and mix
			float vowelL = filteredL + ghost, vowelR = filteredR + ghost;

			// Apply 12dB LPF
			m_voxLPF.updateLowpassCoeff(CutoffToHz(voxCut, m_Nyquist), ResoToQ(voxReso), m_sampleRate);
			m_voxLPF.tick(vowelL, vowelR);
			
			// FIXME
			if (0 == (iSample % voxIntRatio))
			{
				// Apply filter
				m_vowelizerV1.Apply(vowelL, vowelR, vowel);
				
				// Set interpolators
				InterpolatedVowelL.Set(vowelL);
				InterpolatedVowelR.Set(vowelR);
			}
			else
			{
				// Use interpolated result
				vowelL = InterpolatedVowelL.Sample();
				vowelR = InterpolatedVowelR.Sample();
			}

			filteredL = lerpf<float>(filteredL, vowelL, voxWet);
			filteredR = lerpf<float>(filteredR, vowelR, voxWet);

			/*
				Final mix
			*/

			pLeft[iSample]  = lerpf<float>(sampleL, filteredL, wetness);
			pRight[iSample] = lerpf<float>(sampleR, filteredR, wetness);
		}
	}
}
