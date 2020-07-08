
/*
	FM. BISON hybrid FM synthesis -- Post-processing pass.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Auto-wah
	- Delay
	- Yamaha Reface CP-style chorus & phaser
	- Post filter (24dB)
	- Tube distortion
	- Reverb
	- Compressor
	- Master volume, DC blocker & final clamp

	This grew into a huge class; that was of course not the intention but so far it's functional and quite well
	documented so right now (01/07/2020) I see no reason to chop it up
*/

#include "synth-post-pass.h"
#include "synth-stateless-oscillators.h"
#include "synth-distort.h"

namespace SFM
{
	// Remedies sampling artifacts whilst sweeping a delay line
	constexpr float kSweepCutoffHz = 50.f;

	// Oversampling (for 24dB filter & tube distortion)
	constexpr unsigned kOversamplingStages = 2;
	constexpr unsigned kOversamplingFactor = 4;

	// Max. delay feedback (so as not to create an endless loop)
	constexpr float kMaxDelayFeedback = 0.95f; // Like Ableton does, or so I've been told by Paul

	// Delay line size (main delay) & cross bleed amount
	constexpr float kMainDelayLineSize = kMainDelayInSec;
	constexpr float kDelayCrossbleeding = 0.3f;

	PostPass::PostPass(unsigned sampleRate, unsigned maxSamplesPerBlock, unsigned Nyquist) :
		m_sampleRate(sampleRate), m_Nyquist(Nyquist)

		// Delay
,		m_delayLineL(unsigned(sampleRate*kMainDelayLineSize))
,		m_delayLineM(unsigned(sampleRate*kMainDelayLineSize))
,		m_delayLineR(unsigned(sampleRate*kMainDelayLineSize))
,		m_curDelayInSec(0.f, sampleRate, kDefParameterLatency * 4.f /* Longer */)
,		m_curDelayWet(0.f, sampleRate, kDefParameterLatency)
,		m_curDelayFeedback(0.f, sampleRate, kDefParameterLatency)
,		m_curDelayFeedbackCutoff(1.f, sampleRate, kDefParameterLatency)
		
		// CP
,		m_chorusOrPhaser(-1)
,		m_chorusDL(sampleRate/10  /* 100MS max. chorus delay */)
,		m_chorusSweep(sampleRate), m_chorusSweepMod(sampleRate)
,		m_chorusSweepLPF1(kSweepCutoffHz/sampleRate), m_chorusSweepLPF2(kSweepCutoffHz/sampleRate)
,		m_phaserSweep(sampleRate)
,		m_phaserSweepLPF((kSweepCutoffHz*2.f)/sampleRate) // Tweaked a little for effect

		// Oversampling (JUCE)
,		m_oversamplingStages(kOversamplingStages)
,		m_oversamplingFactor(kOversamplingFactor)
,		m_oversamplingRate(sampleRate*m_oversamplingFactor)
,		m_oversampling(2, m_oversamplingStages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true)

		// Post filter
,		m_postFilter(m_oversamplingRate)
,		m_curPostCutoff(0.f, m_oversamplingRate, kDefParameterLatency * 2.f /* Longer */)
,		m_curPostQ(0.f, m_oversamplingRate, kDefParameterLatency)
,		m_curPostDrivedB(0.f, m_oversamplingRate, kDefParameterLatency)
,		m_curPostWet(0.f, m_oversamplingRate, kDefParameterLatency)
		
		// Tube distort
,		m_curTubeDist(0.f, m_oversamplingRate, kDefParameterLatency)
,		m_curTubeDrive(kDefTubeDrive, m_oversamplingRate, kDefParameterLatency)
,		m_curTubeOffset(0.f, m_oversamplingRate, kDefParameterLatency)

		// Low blocker
,		m_lowBlocker(kLowBlockerHz, sampleRate)

		// External effects
,		m_wah(sampleRate, Nyquist)
,		m_reverb(sampleRate, Nyquist)
,		m_compressor(sampleRate)
,		m_compressorBiteLPF(5.f / (sampleRate/maxSamplesPerBlock))
		
		// CP wetness & master volume
,		m_curEffectWet(0.f, sampleRate, kDefParameterLatency)
,		m_curMasterVoldB(kDefVolumedB, sampleRate, kDefParameterLatency)
	{
		// Allocate intermediate buffers
		m_pBufL  = reinterpret_cast<float *>(mallocAligned(maxSamplesPerBlock*sizeof(float), 16));
		m_pBufR  = reinterpret_cast<float *>(mallocAligned(maxSamplesPerBlock*sizeof(float), 16));

		// Initialize JUCE oversampling object
		m_oversampling.initProcessing(maxSamplesPerBlock);
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
	                     float delayInSec, float delayWet, float delayFeedback, float delayFeedbackCutoff,
	                     float postCutoff, float postQ, float postDrivedB, float postWet,
						 float tubeDistort, float tubeDrive, float tubeOffset,
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
		SFM_ASSERT(delayFeedback >= 0.f && delayFeedback <= 1.f);
		SFM_ASSERT(masterVoldB >= kMinVolumedB && masterVoldB <= kMaxVolumedB);
		SFM_ASSERT(tubeDistort >= 0.f && tubeDistort <= 1.f);
		SFM_ASSERT(tubeDrive >= kMinTubeDrive && tubeDrive <= kMaxTubeDrive);
		SFM_ASSERT(tubeOffset >= kMinTubeOffset && tubeOffset <= kMaxTubeOffset);

		// Only adapt the BPM if it fits in the delay line (Ableton does this so why won't we?)
		const bool useBPM = false == (rateBPM < 1.f/kMainDelayInSec);

		// Copy inputs to local buffers (a bit wasteful but in practice this does not involve a whole lot of samples)
		const size_t bufSize = numSamples * sizeof(float);
		memcpy(m_pBufL, pLeftIn,  bufSize);
		memcpy(m_pBufR, pRightIn, bufSize);

#if !defined(SFM_DISABLE_FX)

		/* ----------------------------------------------------------------------------------------------------

			Auto-wah

			Can cause latency, but only if in use (i.e. wet).

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

			// Write to delay line
			const float left     = effectL;
			const float right    = effectR;
			const float monaural = 0.5f*(left+right);
					
			m_delayLineL.Write(left);
			m_delayLineM.Write(monaural);
			m_delayLineR.Write(right);
			
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

			// Mix FX with (filtered) delay
			const float wet = m_curDelayWet.Sample();
			m_pBufL[iSample] = left  + wet*filteredL;
			m_pBufR[iSample] = right + wet*filteredR;
		}

		/* ----------------------------------------------------------------------------------------------------

			Oversampled: 24dB ladder filter, tube distortion

			JUCE says:
			" Choose between FIR or IIR filtering depending on your needs in term of latency and phase 
			  distortion. With FIR filters, the phase is linear but the latency is maximised. With IIR 
			  filtering, the phase is compromised around the Nyquist frequency but the latency is minimised. "

			Currently we use the IIR version for minimal latency.

		 ------------------------------------------------------------------------------------------------------ */

		const auto numOversamples = numSamples*m_oversamplingFactor;

		// Set post filter parameters
		m_curPostCutoff.SetTarget(postCutoff);
		m_curPostQ.SetTarget(postQ);
		m_curPostDrivedB.SetTarget(postDrivedB);
		m_curPostWet.SetTarget(postWet);
		
		// Set tube distortion parameters
		m_curTubeDist.SetTarget(tubeDistort);
		m_curTubeDrive.SetTarget(tubeDrive);
		m_curTubeOffset.SetTarget(tubeOffset);

		// Oversample for this stage
		float *inputBuffers[2] = { m_pBufL, m_pBufR };
		juce::dsp::AudioBlock<float> inputBlock(inputBuffers, 2, numSamples);

		auto outBlock = m_oversampling.processSamplesUp(inputBlock);
		SFM_ASSERT(numOversamples == outBlock.getNumSamples());

		float *pOverL = outBlock.getChannelPointer(0);
		float *pOverR = outBlock.getChannelPointer(1);

		//
		// Apply post filter & tube distortion
		//

		// Anti-aliasing filter (performed on entire signal to cut off all harmonics above the host Nyquist rate)
		m_tubeFilterAA.updateLowpassCoeff(m_Nyquist, kSVF12dBFalloffQ, m_oversamplingRate); 

		for (unsigned iSample = 0; iSample < numOversamples; ++iSample)
		{
			float sampleL = pOverL[iSample]; 
			float sampleR = pOverR[iSample];
				
			// Apply 24dB post filter
			float filteredL = sampleL, filteredR = sampleR;

			const float curPostCutoff = m_curPostCutoff.Sample();
			const float curPostQ = m_curPostQ.Sample();
			const float curPostDrive = dBToGain(m_curPostDrivedB.Sample());
			const float curPostWet = m_curPostWet.Sample();

			m_postFilter.SetParameters(curPostCutoff, curPostQ, curPostDrive);
			m_postFilter.Apply(filteredL, filteredR);

			const float postSampleL = filteredL;
			const float postSampleR = filteredR;

			// Apply distortion				
			filteredL = sampleL; 
			filteredR = sampleR;

			const float amount = m_curTubeDist.Sample();
			const float drive  = m_curTubeDrive.Sample();
			const float offset = m_curTubeOffset.Sample();
			
			filteredL = ClassicCubicClip((offset + filteredL)*drive);
			filteredR = ClassicCubicClip((offset + filteredR)*drive);
			
			// Remove DC offset
			m_tubeDCBlocker.Apply(filteredL, filteredR);
			
			// Mix them
			sampleL += postSampleL*curPostWet;
			sampleR += postSampleR*curPostWet;
				
			sampleL = lerpf<float>(sampleL, sampleL+filteredL, amount);
			sampleR = lerpf<float>(sampleR, sampleR+filteredR, amount);

			// Remove aliasing (originally meant for tube dist., but applied right here to clean up entire signal)
			m_tubeFilterAA.tick(sampleL, sampleR);

			pOverL[iSample] = sampleL;
			pOverR[iSample] = sampleR;
		}

		// Downsample result
		m_oversampling.processSamplesDown(inputBlock);

		// FIXME
//		const float samplingLatency = m_oversampling.getLatencyInSamples();

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

			Introduces latency when 'compLookahead' is larger than zero.

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
		const float sweepMod = fast_cosf(float(m_chorusSweepMod.Sample()));
		
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
		
		// Mix result with dry signal
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
		
		// Mix result with dry signal
		wetness *= kMaxChorusPhaserWet;
		outL = sampleL + wetness*filteredL;
		outR = sampleR + wetness*filteredR;
	}
}
