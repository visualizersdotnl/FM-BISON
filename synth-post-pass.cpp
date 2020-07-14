
/*
	FM. BISON hybrid FM synthesis -- Post-processing pass.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Copy / First AA-filter
	- Auto-wah
	- Delay
	- Yamaha Reface CP-style chorus & phaser
	- Post filter (24dB)          - 4X oversampling
	- Tube distortion             - 4X oversampling
	- Second anti-aliasing filter - 4X oversampling
	- Reverb
	- Compressor
	- Master volume, DC blocker & final clamp

	This grew into a huge class; that was of course not the intention but so far it's functional and quite well
	documented so right now (01/07/2020) I see no reason to chop it up

	However, the amount of processing done every Render() cycle is significant, so I should look into disabling
	certain effects when they do not contribute to the patch (at that particular moment)
*/

#include "synth-post-pass.h"
#include "synth-stateless-oscillators.h"
#include "synth-distort.h"

namespace SFM
{
	// Remedies sampling artifacts whilst sweeping a delay line
	constexpr float kSweepCutoffHz = 50.f;

	// Max. delay feedback (so as not to create an endless loop)
	constexpr float kMaxDelayFeedback = 0.95f; // Like Ableton does, or so I've been told by Paul

	// Delay line size (main delay) & cross bleed amount
	constexpr float kMainDelayLineSize = kMainDelayInSec;
	constexpr float kDelayCrossbleeding = 0.3f;
	
	// The compressor's 'bite' is filtered so it can be used as a GUI indicator; the higher this value the brighter it comes up and quicker it fades
	constexpr float kCompressorBiteCutHz = 480.f;

	PostPass::PostPass(unsigned sampleRate, unsigned maxSamplesPerBlock, unsigned Nyquist) :
		m_sampleRate(sampleRate), m_Nyquist(Nyquist), m_sampleRate4X(sampleRate*4)

		// Delay
,		m_delayLineL(unsigned(sampleRate*kMainDelayLineSize))
,		m_delayLineM(unsigned(sampleRate*kMainDelayLineSize))
,		m_delayLineR(unsigned(sampleRate*kMainDelayLineSize))
,		m_curDelayInSec(0.f, sampleRate, kDefParameterLatency * 4.f /* Longer */)
,		m_curDelayWet(0.f, sampleRate, kDefParameterLatency)
,		m_curDelayDrive(1.f /* 0dB */, sampleRate, kDefParameterLatency)
,		m_curDelayFeedback(0.f, sampleRate, kDefParameterLatency)
,		m_curDelayFeedbackCutoff(1.f, sampleRate, kDefParameterLatency)
		
		// CP
,		m_chorusOrPhaser(-1)
,		m_chorusDL(sampleRate/10  /* 100MS max. chorus delay */)
,		m_chorusSweep(sampleRate), m_chorusSweepMod(sampleRate)
,		m_chorusSweepLPF1(kSweepCutoffHz/sampleRate), m_chorusSweepLPF2(kSweepCutoffHz/sampleRate)
,		m_phaserSweep(sampleRate)
,		m_phaserSweepLPF((kSweepCutoffHz*2.f)/sampleRate) // Tweaked a little for effect

		// Oversampling (stereo)
,		m_oversampling4X(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true)

		// Post filter
,		m_postFilter(m_sampleRate4X)
,		m_curPostCutoff(0.f, m_sampleRate4X, kDefParameterLatency * 2.f /* Longer */)
,		m_curPostReso(0.f, m_sampleRate4X, kDefParameterLatency)
,		m_curPostDrive(0.f, m_sampleRate4X, kDefParameterLatency)
,		m_curPostWet(0.f, m_sampleRate4X, kDefParameterLatency)
		
		// Tube distort
,		m_curTubeDist(0.f, m_sampleRate4X, kDefParameterLatency)
,		m_curTubeDrive(kDefTubeDrive, m_sampleRate4X, kDefParameterLatency)
,		m_curTubeOffset(0.f, m_sampleRate4X, kDefParameterLatency)

		// Low blocker
,		m_lowBlocker(kLowBlockerHz, sampleRate)

		// External effects
,		m_wah(sampleRate, Nyquist)
,		m_reverb(sampleRate, Nyquist)
,		m_compressor(sampleRate)
,		m_compressorBiteLPF(kCompressorBiteCutHz/sampleRate)
		
		// CP wetness & master volume
,		m_curEffectWet(0.f, sampleRate, kDefParameterLatency)
,		m_curMasterVoldB(kDefVolumedB, sampleRate, kDefParameterLatency)
	{
		// Allocate intermediate buffers
		m_pBufL  = reinterpret_cast<float *>(mallocAligned(maxSamplesPerBlock*sizeof(float), 16));
		m_pBufR  = reinterpret_cast<float *>(mallocAligned(maxSamplesPerBlock*sizeof(float), 16));

		// Initialize JUCE oversampling object (FIXME)
		m_oversampling4X.initProcessing(maxSamplesPerBlock);
	}

	PostPass::~PostPass()
	{
		freeAligned(m_pBufL);
		freeAligned(m_pBufR);
	}

	void PostPass::Apply(unsigned numSamples,
	                     float rateBPM,
	                     float wahResonance, float wahAttack, float wahHold, float wahRate, float wahDrivedB, float wahSpeak, float wahSpeakVowel, float wahSpeakVowelMod, float wahSpeakGhost, float wahSpeakCut, float wahSpeakReso, float wahCut, float wahWet,
	                     float cpRate, float cpWet, bool isChorus,
	                     float delayInSec, float delayWet, float delayDrivedB, float delayFeedback, float delayFeedbackCutoff,
	                     float postCutoff, float postReso, float postDrivedB, float postWet,
						 float tubeDistort, float tubeDrive, float tubeOffset,
						 float antiAliasing,
	                     float reverbWet, float reverbRoomSize, float reverbDampening, float reverbWidth, float reverbLP, float reverbHP, float reverbPreDelay,
	                     float compThresholddB, float compKneedB, float compRatio, float compGaindB, float compAttack, float compRelease, float compLookahead, bool compAutoGain, float compRMSToPeak,
	                     float masterVoldB,
	                     const float *pLeftIn, const float *pRightIn, float *pLeftOut, float *pRightOut)
	{
		// Other parameters should be checked in functions they're passed to; however, this list can be incomplete; when
		// running into such a situation promptly fix it!
		SFM_ASSERT(nullptr != pLeftIn  && nullptr != pRightIn);
		SFM_ASSERT(nullptr != pLeftOut && nullptr != pRightOut);
		SFM_ASSERT(numSamples > 0);
		SFM_ASSERT(rateBPM >= 0.f);
		SFM_ASSERT(cpRate >= 0.f && cpRate <= 1.f);
		SFM_ASSERT(cpWet  >= 0.f && cpWet  <= 1.f);
		SFM_ASSERT(delayInSec >= 0.f && delayInSec <= kMainDelayInSec);
		SFM_ASSERT(delayWet >= 0.f && delayWet <= 1.f);
		SFM_ASSERT(delayDrivedB >= kMinDelayDrivedB && delayDrivedB <= kMaxDelayDrivedB);
		SFM_ASSERT(delayFeedback >= 0.f && delayFeedback <= 1.f);
		SFM_ASSERT(postCutoff >= 0.f && postCutoff <= 1.f);
//		SFM_ASSERT(postReso >= 0.f && postReso <= 1.f);
		SFM_ASSERT(masterVoldB >= kMinVolumedB && masterVoldB <= kMaxVolumedB);
		SFM_ASSERT(tubeDistort >= 0.f && tubeDistort <= 1.f);
		SFM_ASSERT(tubeDrive >= kMinTubeDrive && tubeDrive <= kMaxTubeDrive);
		SFM_ASSERT(antiAliasing >= 0.f && antiAliasing <= 1.f);
		SFM_ASSERT(tubeOffset >= kMinTubeOffset && tubeOffset <= kMaxTubeOffset);

		// Only adapt the BPM if it fits in the delay line (Ableton does this so why won't we?)
		const bool useBPM = false == (rateBPM < 1.f/kMainDelayInSec);

		/* ----------------------------------------------------------------------------------------------------

			Copy samples to local buffers

		 ------------------------------------------------------------------------------------------------------ */

		const size_t bufSize = numSamples * sizeof(float);

		memcpy(m_pBufL, pLeftIn,  bufSize);
		memcpy(m_pBufR, pRightIn, bufSize);

#if !defined(SFM_DISABLE_FX)

		/* ----------------------------------------------------------------------------------------------------

			Auto-wah

		 ------------------------------------------------------------------------------------------------------ */

		if (true == useBPM)
			wahRate = rateBPM; // Tested, works fine!

		m_wah.SetParameters(wahResonance, wahAttack, wahHold, wahRate, wahDrivedB, wahSpeak, wahSpeakVowel, wahSpeakVowelMod, wahSpeakGhost, wahSpeakCut, wahSpeakReso, wahCut, wahWet);
		m_wah.Apply(m_pBufL, m_pBufR, numSamples, false == useBPM);

		/* ----------------------------------------------------------------------------------------------------

			Chorus/Phaser + Delay

			Mixed on top of original signal, does not introduce latency.

		 ------------------------------------------------------------------------------------------------------ */

		// Effect type switch?
		const bool effectSwitch = m_chorusOrPhaser != int(isChorus);

		// Set effect wetness target
		const auto curEffectWet = m_curEffectWet.Get(); // FIXME: necessary due to SetRate()
		m_curEffectWet.SetRate(numSamples);

		if (true == effectSwitch)
		{
			// Fade out FX before switching
			m_curEffectWet.Set(curEffectWet);
			m_curEffectWet.SetTarget(0.f);
		}
		else
		{
			m_curEffectWet.Set(curEffectWet);
			m_curEffectWet.SetTarget(cpWet);
		}
	
		// Calculate delay & set delay mode
		const float delay = (false == useBPM) ? delayInSec : 1.f/rateBPM;
		SFM_ASSERT(delay >= 0.f && delay <= kMainDelayInSec);

		m_curDelayInSec.SetTarget(delay);
		m_curDelayWet.SetTarget(delayWet);
		m_curDelayDrive.SetTarget(dB2Lin(delayDrivedB));
		m_curDelayFeedback.SetTarget(delayFeedback);
		m_curDelayFeedbackCutoff.SetTarget(delayFeedbackCutoff);
		
		// Set rate for both effects
		if (false == useBPM) // Sync. to BPM?
		{
			// No, use manual setting
			SetChorusRate(cpRate, kMaxChorusRate);
			SetPhaserRate(cpRate, kMaxPhaserRate);
		}
		else
		{
			// Locked to BPM
			static_assert(kMaxChorusRate >= kMaxPhaserRate);
			SetChorusRate(rateBPM, kMaxChorusRate/kMaxPhaserRate);
			SetPhaserRate(rateBPM, 1.f);
		}

		// Set up function call to apply desired effect (chorus or phaser)
		using namespace std::placeholders;
		const std::function<void(float, float, float&, float&, float)> effectFunc((1 /* Chorus */ == m_chorusOrPhaser)
			? std::bind(&PostPass::ApplyChorus, this, _1, _2, _3, _4, _5)
			: std::bind(&PostPass::ApplyPhaser, this, _1, _2, _3, _4, _5));
				
		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			const float sampleL = m_pBufL[iSample];
			const float sampleR = m_pBufR[iSample];
				
			// Always feed the chorus' delay line
			// This approach has it's flaws: https://matthewvaneerde.wordpress.com/2010/12/07/downmixing-stereo-to-mono/
			m_chorusDL.Write(sampleL*0.5f + sampleR*0.5f);
			
			// Apply effect
			float effectL, effectR;
			const float effectWet = m_curEffectWet.Sample();
			effectFunc(sampleL, sampleR, effectL, effectR, effectWet);

			// Apply delay
			const float curDelayInSec = m_curDelayInSec.Sample();
			SFM_ASSERT(curDelayInSec >= 0.f && curDelayInSec <= kMainDelayInSec);

			// Write driven samples to delay line
			const float left     = effectL;
			const float right    = effectR;
			const float monaural = 0.5f*(left+right);
			
			const float drive = m_curDelayDrive.Sample();
			m_delayLineL.Write(left     * drive);
			m_delayLineM.Write(monaural * drive);
			m_delayLineR.Write(right    * drive);
			
			// Sample delay line
			const float curDelay = curDelayInSec/kMainDelayInSec;
			const float delayedL = m_delayLineL.ReadNormalized(curDelay);
			const float delayedM = m_delayLineM.ReadNormalized(curDelay);
			const float delayedR = m_delayLineR.ReadNormalized(curDelay);
			
			// Bleed delay samples a bit
			constexpr float crossBleedAmt = kDelayCrossbleeding;
			constexpr float invCrossBleedAmt = 1.f-crossBleedAmt;
			const float crossBleed = delayedM;
			const float delayL = delayedL*invCrossBleedAmt + crossBleed*crossBleedAmt;
			const float delayR = delayedR*invCrossBleedAmt + crossBleed*crossBleedAmt;

			// Filter delay
			const float curFc = (m_curDelayFeedbackCutoff.Sample() * m_Nyquist/8)/m_sampleRate; // Limited range (8th of Nyquist) gives a more pronounced effect
			m_delayFeedbackLPF_L.SetCutoff(curFc);
			m_delayFeedbackLPF_R.SetCutoff(curFc);

			const float filteredL = m_delayFeedbackLPF_L.Apply(delayL);
			const float filteredR = m_delayFeedbackLPF_R.Apply(delayR);

			const float filteredM = 0.5f*(filteredL+filteredR);

			// Feed back into delay line
			const float curFeedback =  m_curDelayFeedback.Sample()*kMaxDelayFeedback;
			m_delayLineL.WriteFeedback(filteredL, curFeedback);
			m_delayLineM.WriteFeedback(filteredM, curFeedback);
			m_delayLineR.WriteFeedback(filteredR, curFeedback);

			// Add (filtered, FIXME?) delay
			const float wet = m_curDelayWet.Sample();
			m_pBufL[iSample] = left  + wet*filteredL;
			m_pBufR[iSample] = right + wet*filteredR;
		}

		/* ----------------------------------------------------------------------------------------------------

			Oversampled: 24dB ladder filter, tube distortion & AA (4X)

			JUCE says:
			" Choose between FIR or IIR filtering depending on your needs in term of latency and phase 
			  distortion. With FIR filters, the phase is linear but the latency is maximised. With IIR 
			  filtering, the phase is compromised around the Nyquist frequency but the latency is minimised. "

			Currently we use the IIR version for minimal latency.

		 ------------------------------------------------------------------------------------------------------ */

		// Set post filter parameters
		m_curPostCutoff.SetTarget(postCutoff);
		m_curPostReso.SetTarget(postReso);
		m_curPostDrive.SetTarget(dB2Lin(postDrivedB));
		m_curPostWet.SetTarget(postWet);

		// Set tube distortion parameters
		m_curTubeDist.SetTarget(tubeDistort);
		m_curTubeDrive.SetTarget(tubeDrive);
		m_curTubeOffset.SetTarget(tubeOffset);

		// Anti-aliasing filter: attenuates frequencies below the host sample rate (distortion)
		const double tubeCutFc = m_sampleRate / double(m_sampleRate4X);
		m_tubeFilterAA_L.setBiquad(bq_type_lowpass, tubeCutFc, kDefGainAtCutoff, 0.0);
		m_tubeFilterAA_L.setBiquad(bq_type_lowpass, tubeCutFc, kDefGainAtCutoff, 0.0);

		// Anti-aliasing filter: attenuates the top end
		const double antiAliasingCutHz = lerpf<double>(m_sampleRate /* SR */, double(m_Nyquist) /* Nyquist 1X */, antiAliasing);
		const double antiAliasingCutFc = antiAliasingCutHz / m_sampleRate4X;
		m_finalFilterAA_L.setBiquad(bq_type_lowpass, antiAliasingCutFc, 0.3, 0.0);
		m_finalFilterAA_R.setBiquad(bq_type_lowpass, antiAliasingCutFc, 0.3, 0.0);
		
		// Main buffers
		float *inputBuffers[2] = { m_pBufL, m_pBufR };
		juce::dsp::AudioBlock<float> inputBlock(inputBuffers, 2, numSamples);

		// Oversample 4X
		auto outBlock = m_oversampling4X.processSamplesUp(inputBlock);
		const size_t numOversamples = outBlock.getNumSamples();
		SFM_ASSERT(numOversamples == numSamples*4);

		float *pOverL = outBlock.getChannelPointer(0);
		float *pOverR = outBlock.getChannelPointer(1);

		for (unsigned iSample = 0; iSample < numOversamples; ++iSample)
		{
			float sampleL = pOverL[iSample]; 
			float sampleR = pOverR[iSample];

			// Apply final AA filter
			sampleL = m_finalFilterAA_L.process(sampleL);
			sampleR = m_finalFilterAA_R.process(sampleR);

			// Apply 24dB post filter
			const float curPostCutoff = m_curPostCutoff.Sample();
			const float curPostReso   = m_curPostReso.Sample();
			const float curPostDrive  = m_curPostDrive.Sample();
			const float curPostWet    = m_curPostWet.Sample();

			float filteredL = sampleL, filteredR = sampleR;

			m_postFilter.SetParameters(kMinPostFilterCutoffHz + curPostCutoff*kPostFilterCutoffRange, curPostReso /* [0..1] */, curPostDrive);
			m_postFilter.Apply(filteredL, filteredR);

			// Mix
			const float postL = lerpf<float>(sampleL, filteredL, curPostWet);
			const float postR = lerpf<float>(sampleR, filteredR, curPostWet);
			
			// Apply (cubic) distortion
			const float amount = m_curTubeDist.Sample();
			const float drive  = m_curTubeDrive.Sample();
			const float offset = m_curTubeOffset.Sample();
			
			float distortedL = postL, distortedR = postR;
			distortedL = ClassicCubicClip((offset+distortedL)*drive);
			distortedR = ClassicCubicClip((offset+distortedR)*drive);
			
			// Remove possible DC offset
			m_tubeDCBlocker.Apply(distortedL, distortedR);

			// Apply distortion AA filter
			m_tubeFilterAA_L.process(distortedL);
			m_tubeFilterAA_R.process(distortedR);
			
			// Mix results
			sampleL = lerpf<float>(postL, distortedL, amount);
			sampleR = lerpf<float>(postR, distortedR, amount);

			// Write
			pOverL[iSample] = sampleL;
			pOverR[iSample] = sampleR;
		}
	
		// Downsample result
		m_oversampling4X.processSamplesDown(inputBlock);


		/* ----------------------------------------------------------------------------------------------------

			Reverb

			Could possibly introduce minor latency; please check (FIXME).

		 ------------------------------------------------------------------------------------------------------ */

		// Apply reverb (after post filter to avoid muddy sound)
		m_reverb.SetRoomSize(reverbRoomSize);
		m_reverb.SetDampening(reverbDampening);
		m_reverb.SetWidth(reverbWidth);
		m_reverb.SetPreDelay(reverbPreDelay);
		m_reverb.Apply(m_pBufL, m_pBufR, numSamples, reverbWet, reverbLP, reverbHP);

		/* ----------------------------------------------------------------------------------------------------

			Compressor

			Causes latency when 'compLookahead' is larger than zero.

		 ------------------------------------------------------------------------------------------------------ */

		 m_compressor.SetParameters(compThresholddB, compKneedB, compRatio, compGaindB, compAttack, compRelease, compLookahead);
		 m_compressorBiteLPF.Apply(m_compressor.Apply(m_pBufL, m_pBufR, numSamples, compAutoGain, compRMSToPeak));

#endif

		/* ----------------------------------------------------------------------------------------------------

			Final pass: blockers, master volume, safety clamp

		 ------------------------------------------------------------------------------------------------------ */

		// Set master volume target
		m_curMasterVoldB.SetTarget(masterVoldB);

		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			float sampleL = m_pBufL[iSample];
			float sampleR = m_pBufR[iSample];

			m_lowBlocker.Apply(sampleL, sampleR);
			m_DCBlocker.Apply(sampleL, sampleR);

			const float gain = dB2Lin(m_curMasterVoldB.Sample());
			sampleL *= gain;
			sampleR *= gain;

			// Clamp(): we won't assume anything about the host's take on output outside [-1..1]
			pLeftOut[iSample]  = Clamp(sampleL);
			pRightOut[iSample] = Clamp(sampleR);
		}

#if !defined(SFM_DISABLE_FX)

		// Reset Chorus/Phaser state
		if (true == effectSwitch)
		{
			if (false == isChorus)
			{
				// Phaser
				for (auto &filter : m_allpassFilters)
					filter.resetState();
			}

			// Chorus delay line is fed continously
		}

#endif

		m_chorusOrPhaser = isChorus;
	}

	/* ----------------------------------------------------------------------------------------------------

		Chorus/Phaser impl.

		FIXME: these are *old* and could use a review or rewrite

	 ------------------------------------------------------------------------------------------------------ */

	void PostPass::ApplyChorus(float sampleL, float sampleR, float &outL, float &outR, float wetness)
	{
		// Sweep modulation LFO
		const float sweepMod = fast_cosf(m_chorusSweepMod.Sample());
		
		// Sweep LFOs
		const float phase = float(m_chorusSweep.Sample());
		
		// This violates my [0..1] phase rule but Kusma's function handles it just fine
		const float sweepL = 0.5f*fast_sinf(phase+sweepMod);
		const float sweepR = 0.5f*fast_sinf((1.f-phase)+sweepMod);
		
		// 2 sweeping samples (FIXME: parametrize, though it's not a priority)
		const float delay  = m_sampleRate*0.005f; // 50MS delay
		const float spread = m_sampleRate*0.003f; // Sweep 30MS
		
		SFM_ASSERT(delay  < m_chorusDL.size());
		SFM_ASSERT(spread < m_chorusDL.size());
		
		// Take sweeped L/R samples (filtered to circumvent artifacts)
		const float chorusL = m_chorusDL.Read(delay + spread*m_chorusSweepLPF1.Apply(sweepL));
		const float chorusR = m_chorusDL.Read(delay + spread*m_chorusSweepLPF2.Apply(sweepR));
		
		// Add result to dry signal
		wetness *= kMaxChorusPhaserWet;
		outL = sampleL + wetness*chorusL; 
		outR = sampleR + wetness*chorusR; 
	}

	void PostPass::ApplyPhaser(float sampleL, float sampleR, float &outL, float &outR, float wetness)
	{
		// Sweep LFO (filtered for pleasing effect)
		const float sweepMod = m_phaserSweepLPF.Apply(oscTriangle(m_phaserSweep.Sample()));
		
		// Sweep cutoff frequency around center
		constexpr float range = 0.2f;
		static_assert(range < 0.5f);
		const float normCutoff = 0.5f + range*sweepMod;
		
		// Start with dry sample
		float filteredL = sampleL;
		float filteredR = sampleR;

		// Cutoff & Q
		const float cutoffHz = SVF_CutoffToHz(normCutoff, m_Nyquist);
		float Q = kSVFLowestFilterQ; // FIXME: use higher Q?
		
		// Apply cascading filters
		for (auto &filter : m_allpassFilters)
		{
			filter.updateAllpassCoeff(cutoffHz, Q, m_sampleRate);
			filter.tick(filteredL, filteredR);

			// Adds a little "space"
			Q += Q;
		}
		
		// Add result to dry signal
		wetness *= kMaxChorusPhaserWet;
		outL = sampleL + wetness*filteredL;
		outR = sampleR + wetness*filteredR;
	}
}
