
/*
	FM. BISON hybrid FM synthesis -- Post-processing pass.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Delay
	- Yamaha Reface CP-style chorus & phaser
	- Reverb
	- Post filter (24dB)
	- Tube amp. distortion
	- Master volume
	- Compressor
	- DC blocker
*/

#include "synth-post-pass.h"
#include "synth-stateless-oscillators.h"
#include "synth-distort.h"

namespace SFM
{
	// Remedies sampling artifacts whilst sweeping a delay line
	const float kSweepCutoffHz = 50.f;

	// Oversampling factor for 24dB MOOG filter & tube amp. distortion
	const unsigned kOversampleStages = 2;
	const unsigned kOversample = 4; // 2^kOversampleStages

	// Max. delay feedback (so as not to create an endless loop)
	const float kMaxDelayFeedback = 0.95f; // Like Ableton does, or so I've been told by Paul

	// Delay line size (main delay)
	const float kMainDelayLineSize = kMainDelayInSec;

	PostPass::PostPass(unsigned sampleRate, unsigned maxSamplesPerBlock, unsigned Nyquist) :
		m_sampleRate(sampleRate), m_Nyquist(Nyquist)

		// Delay
,		m_delayLineL(unsigned(sampleRate*kMainDelayLineSize))
,		m_delayLineM(unsigned(sampleRate*kMainDelayLineSize))
,		m_delayLineR(unsigned(sampleRate*kMainDelayLineSize))
,		m_curDelay(0.f, sampleRate, kDefParameterLatency * 4.f /* Slower */)
,		m_curDelayWet(0.f, sampleRate, kDefParameterLatency)
,		m_curDelayFeedback(0.f, sampleRate, kDefParameterLatency)
,		m_curDelayFeedbackCutoff(1.f, sampleRate, kDefParameterLatency)
		
		// CP
,		m_chorusOrPhaser(-1)
,		m_chorusDL(sampleRate/10  /* 100ms. max. chorus delay */)
,		m_chorusSweep(sampleRate), m_chorusSweepMod(sampleRate)
,		m_chorusSweepLPF1(kSweepCutoffHz/sampleRate), m_chorusSweepLPF2(kSweepCutoffHz/sampleRate)
,		m_phaserSweep(sampleRate)
,		m_phaserSweepLPF((kSweepCutoffHz*2.f)/sampleRate) // Tweaked a little for effect

		// Up- and downsampler (JUCE)
,		m_oversamplingRate(sampleRate*kOversample)
,		m_oversamplingL(1, kOversampleStages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true)
,		m_oversamplingR(1, kOversampleStages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true)

		// Post filter
,		m_postFilter(m_oversamplingRate)
,		m_curPostCutoff(0.f, m_oversamplingRate, kDefParameterLatency)
,		m_curPostQ(0.f, m_oversamplingRate, kDefParameterLatency)
,		m_curPostDrivedB(0.f, m_oversamplingRate, kDefParameterLatency)
,		m_curPostWet(0.f, m_oversamplingRate, kDefParameterLatency)
		
		// Tube amp. distort
,		m_curAvgVelocity(0.f, m_oversamplingRate, kDefParameterLatency * 2.f /* Slower! */)
,		m_curTubeDist(0.f, m_oversamplingRate, kDefParameterLatency)
,		m_curTubeDrive(kDefTubeDrivedB, m_oversamplingRate, kDefParameterLatency)

		// DC blocker
,		m_lowCutFilter(kLowCutHz, sampleRate)

		// Reverb impl.
,		m_reverb(sampleRate, Nyquist)

		// Compressor
,		m_compressor(sampleRate)
,		m_curCompPeakToRMS(kDefCompPeakToRMS, sampleRate, kDefParameterLatency)
,		m_curCompThresholddB(kDefCompThresholddB, sampleRate, kDefParameterLatency)
,		m_curCompKneedB(kDefCompKneedB, sampleRate, kDefParameterLatency)
,		m_curCompRatio(kDefCompRatio, sampleRate, kDefParameterLatency)
,		m_curCompGaindB(kDefCompGaindB, sampleRate, kDefParameterLatency)
,		m_curCompAttack(kDefCompAttack, sampleRate, kDefParameterLatency)
,		m_curCompRelease(kDefCompRelease, sampleRate, kDefParameterLatency)

		// Wahwah
,		m_wah(sampleRate, Nyquist)
		
		// CP wetness & master volume
,		m_curEffectWet(0.f, sampleRate, kDefParameterLatency)
,		m_curMasterVol(kDefVolumedB, sampleRate, kDefParameterLatency)
	{
		// Allocate intermediate buffers
		m_pBufL  = reinterpret_cast<float *>(mallocAligned(maxSamplesPerBlock*sizeof(float), 16));
		m_pBufR  = reinterpret_cast<float *>(mallocAligned(maxSamplesPerBlock*sizeof(float), 16));

		// Initialize JUCE (over)sampling objects
		m_oversamplingL.initProcessing(maxSamplesPerBlock);
		m_oversamplingR.initProcessing(maxSamplesPerBlock);

		// Reset tube amp. filter(s)
		m_tubeFilterPre.resetState();
		m_tubeFilterPost.resetState();
	}

	PostPass::~PostPass()
	{
		freeAligned(m_pBufL);
		freeAligned(m_pBufR);
	}

	void PostPass::Apply(unsigned numSamples,
	                     float rateBPM,
	                     float wahSlack, float wahSpeed, float wahHold, float wahRate, float wahSpeak, float wahCut, float wahWet,
	                     float cpRate, float cpWet, bool isChorus,
	                     float delayInSec, float delayWet, float delayFeedback, float delayFeedbackCutoff,
	                     float postCutoff, float postQ, float postDrivedB, float postWet,
						 float avgVelocity, float tubeDistort, float tubeDrivedB,
	                     float reverbWet, float reverbRoomSize, float reverbDampening, float reverbWidth, float reverbLP, float reverbHP, float reverbPreDelay,
	                     float compPeakToRMS, float compThresholddB, float compKneedB, float compRatio, float compGaindB, float compAttack, float compRelease, float compLookahead,
	                     float masterVol,
	                     const float *pLeftIn, const float *pRightIn, float *pLeftOut, float *pRightOut)
	{
		// Other parameters are checked in functions they're passed to!
		// FIXME: is this complete?
		SFM_ASSERT(nullptr != pLeftIn  && nullptr != pRightIn);
		SFM_ASSERT(nullptr != pLeftOut && nullptr != pRightOut);
		SFM_ASSERT(numSamples > 0);
		SFM_ASSERT(rateBPM >= 0.f); // FIXME
		SFM_ASSERT(cpRate >= 0.f && cpRate <= 1.f);
		SFM_ASSERT(cpWet  >= 0.f && cpWet  <= 1.f);
		SFM_ASSERT(delayInSec >= 0.f);
		SFM_ASSERT(delayWet >= 0.f && delayWet <= 1.f);
		SFM_ASSERT(delayFeedback >= 0.f && delayFeedback <= 1.f);
		SFM_ASSERT(masterVol >= kMinVolumedB && masterVol <= kMaxVolumedB);
		SFM_ASSERT(avgVelocity >= 0.f && avgVelocity <= 1.f);
		SFM_ASSERT(tubeDistort >= 0.f && tubeDistort <= 1.f);

		// Hack for 0.9 patches (FIXME!)
		if (tubeDrivedB < kMinTubeDrivedB)
			tubeDrivedB = 16.f;

		SFM_ASSERT(tubeDrivedB >= kMinTubeDrivedB && tubeDrivedB <= kMaxTubeDrivedB);

		// Only adapt the BPM if it fits in the delay line (Ableton does this so why won't we?)
		const bool useBPM = false == (rateBPM < 1.f/kMainDelayInSec);

		/* ----------------------------------------------------------------------------------------------------

			Auto-wah

		 ------------------------------------------------------------------------------------------------------ */

		m_curWahSlack.SetTarget(wahSlack);
		m_curWahSpeed.SetTarget(wahSpeed);
		m_curWahHold.SetTarget(wahHold);
		m_curWahRate.SetTarget(wahRate);
		m_curWahSpeak.SetTarget(wahSpeak);
		m_curWahCut.SetTarget(wahCut);
		m_curWahWet.SetTarget(wahWet);

		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			float sampleL = *pLeftIn++;
			float sampleR = *pRightIn++;
//			float sampleL = m_pBufL[iSample];
//			float sampleR = m_pBufR[iSample];
			
			m_wah.SetParameters(
				m_curWahSlack.Sample(),
				m_curWahSpeed.Sample(),
				m_curWahHold.Sample(),
				m_curWahRate.Sample(),
				m_curWahSpeak.Sample(),
				m_curWahCut.Sample(),
				m_curWahWet.Sample()
			);

			m_wah.Apply(sampleL, sampleR);

			m_pBufL[iSample] = sampleL;
			m_pBufR[iSample] = sampleR;
		}

		/* ----------------------------------------------------------------------------------------------------

			Chorus/Phaser + Delay

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

		m_curDelay.SetTarget(delay);
		m_curDelayWet.SetTarget(delayWet);
		m_curDelayFeedback.SetTarget(delayFeedback);
		m_curDelayFeedbackCutoff.SetTarget(delayFeedbackCutoff);
		
		// Set rate for both effects
		if (false == useBPM) // Sync. to BPM?
		{
			// No, use manual setting
			SetChorusRate(cpRate, kMaxChorusSpeed);
			SetPhaserRate(cpRate, kMaxPhaserSpeed);
		}
		else
		{
			// Locked to BPM
			SFM_ASSERT(kMaxChorusSpeed >= kMaxPhaserSpeed);
			SetChorusRate(rateBPM, kMaxChorusSpeed/kMaxPhaserSpeed);
			SetPhaserRate(rateBPM, 1.f);
		}

		// Set up function call to apply desired effect (chorus or phaser)
		using namespace std::placeholders;
		const std::function<void(float, float, float&, float&, float)> effectFunc((1 /* Chorus */ == m_chorusOrPhaser)
			? std::bind(&PostPass::ApplyChorus, this, _1, _2, _3, _4, _5)
			: std::bind(&PostPass::ApplyPhaser, this, _1, _2, _3, _4, _5));
				
		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
//			const float sampleL = *pLeftIn++;
//			const float sampleR = *pRightIn++;
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
			const float curDelay = m_curDelay.Sample();
			SFM_ASSERT(curDelay >= 0.f && curDelay <= kMainDelayInSec);

			const float left     = effectL;
			const float right    = effectR;
			const float monaural = kMaxDelayFeedback*(left*0.5f + right*0.5f);
					
			m_delayLineL.Write(left);
			m_delayLineM.Write(monaural);
			m_delayLineR.Write(right);

			const float delaySamples = m_sampleRate*curDelay;
			const float delayedL = m_delayLineL.Read(delaySamples);
			const float delayedM = m_delayLineM.Read(delaySamples);
			const float delayedR = m_delayLineR.Read(delaySamples);

			// FIXME: parameter?
			const float crossBleedAmt = 0.05f;
			const float invCrossBleedAmt = 1.f-crossBleedAmt;
			const float crossBleed = /* fast_tanhf */ (delayedL+delayedM+delayedR);
			const float finalL = delayedL*invCrossBleedAmt + crossBleed*crossBleedAmt;
			const float finalR = delayedR*invCrossBleedAmt + crossBleed*crossBleedAmt;
			
			// Feed back
			const float curCutoff = CutoffToHz(m_curDelayFeedbackCutoff.Sample(), m_Nyquist);
			m_delayLineL.SetFeedbackCutoff(curCutoff, m_sampleRate);
			m_delayLineM.SetFeedbackCutoff(curCutoff, m_sampleRate);
			m_delayLineR.SetFeedbackCutoff(curCutoff, m_sampleRate);

			const float curFeedbackDry = m_curDelayFeedback.Sample();
			const float curFeedback    = (curDelay/kMainDelayInSec)*curFeedbackDry*kMaxDelayFeedback;
			
			m_delayLineL.WriteFeedback(finalL, curFeedback);
			m_delayLineR.WriteFeedback(finalR, curFeedback);

			// Mix FX with delay
			const float wet = m_curDelayWet.Sample();
			m_pBufL[iSample] = left  + wet*finalL;
			m_pBufR[iSample] = right + wet*finalR;
		}

		/* ----------------------------------------------------------------------------------------------------

			Oversampled: 24dB ladder filter, tube amp. distortion

		 ------------------------------------------------------------------------------------------------------ */

		// Set post filter parameters
		m_curPostCutoff.SetTarget(postCutoff);
		m_curPostQ.SetTarget(postQ);
		m_curPostDrivedB.SetTarget(postDrivedB);
		m_curPostWet.SetTarget(postWet);
		
		// Set tube dist. parameters
		m_curAvgVelocity.SetTarget(avgVelocity);
		m_curTubeDist.SetTarget(tubeDistort);
		m_curTubeDrive.SetTarget(dBToGain(tubeDrivedB));

		const auto numOversamples = numSamples*kOversample; 

		{
			// Oversample L/R for the coming steps
			juce::dsp::AudioBlock<float> inputBlockL(&m_pBufL, 1, numSamples);
			juce::dsp::AudioBlock<float> inputBlockR(&m_pBufR, 1, numSamples);

			auto outBlockL = m_oversamplingL.processSamplesUp(inputBlockL);
			auto outBlockR = m_oversamplingR.processSamplesUp(inputBlockR);

			SFM_ASSERT(numOversamples == outBlockL.getNumSamples());

			float *pOverL = outBlockL.getChannelPointer(0);
			float *pOverR = outBlockR.getChannelPointer(0);

			// Apply post filter & tube amp. distortion
			m_tubeFilterPre.updateCoefficients(3000.0, 0.05, SvfLinearTrapOptimised2::LOW_PASS_FILTER, m_oversamplingRate);
			m_tubeFilterPost.updateCoefficients(2300.0, 0.2, SvfLinearTrapOptimised2::LOW_PASS_FILTER, m_oversamplingRate);

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

				const float postSampleL = lerpf<float>(sampleL, filteredL, curPostWet);
				const float postSampleR = lerpf<float>(sampleR, filteredR, curPostWet);
				
				// Apply distortion
				filteredL = sampleL; filteredR = sampleR;

				const float amount      = smoothstepf(m_curTubeDist.Sample());
				const float velocity    = powf(m_curAvgVelocity.Sample(), 3.f);
				const float drive       = m_curTubeDrive.Sample() + dBToGain(velocity*3.f);

				m_tubeFilterPre.tick(filteredL, filteredR);
				filteredL *= drive*amount;
				filteredR *= drive*amount;
				filteredL = ZoelzerClip(filteredL);
				filteredR = ZoelzerClip(filteredR);
				m_tubeFilterPost.tick(filteredL, filteredR);
				
				const float clipGain = dBToGain(-6.f + 9.f*amount*velocity);

				// Blend 2 effects with dry sample
				pOverL[iSample] = postSampleL + lerpf<float>(sampleL, clipGain*filteredL, amount);
				pOverR[iSample] = postSampleR + lerpf<float>(sampleR, clipGain*filteredR, amount);
			}

			// Downsample (result); do I need to reset? (FIXME)
			m_oversamplingL.processSamplesDown(inputBlockL);
			m_oversamplingR.processSamplesDown(inputBlockR);
		}

		/* ----------------------------------------------------------------------------------------------------

			Reverb

		 ------------------------------------------------------------------------------------------------------ */

		// Apply reverb (after post filter to avoid muddy sound)
		m_reverb.SetRoomSize(reverbRoomSize);
		m_reverb.SetDampening(reverbDampening);
		m_reverb.SetWidth(reverbWidth);
		m_reverb.SetPreDelay(reverbPreDelay);
		m_reverb.Apply(m_pBufL, m_pBufR, numSamples, reverbWet, reverbLP, reverbHP);

		/* ----------------------------------------------------------------------------------------------------

			DC blocker, compressor, master value

		 ------------------------------------------------------------------------------------------------------ */

		// Set compressor parameters
		m_curCompPeakToRMS.SetTarget(compPeakToRMS);
		m_curCompThresholddB.SetTarget(compThresholddB);
		m_curCompKneedB.SetTarget(compKneedB);
		m_curCompRatio.SetTarget(compRatio);
		m_curCompGaindB.SetTarget(compGaindB);
		m_curCompAttack.SetTarget(compAttack);
		m_curCompRelease.SetTarget(compRelease);
		m_curCompLookahead.SetTarget(compLookahead);

		// Set master volume target
		m_curMasterVol.SetTarget(masterVol);

//		m_compressor.SetParameters(..., compThresholddB, compKneedB, compRatio, compGaindB, compAttack, compRelease);
		// ^^ This is obviously a tad quicker, but I've explicitly chosen for fully click-free parameters

		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			float sampleL = m_pBufL[iSample];
			float sampleR = m_pBufR[iSample];

			m_compressor.SetParameters(
				m_curCompPeakToRMS.Sample(),
				m_curCompThresholddB.Sample(),
				m_curCompKneedB.Sample(),
				m_curCompRatio.Sample(),
				m_curCompGaindB.Sample(),
				m_curCompAttack.Sample(),
				m_curCompRelease.Sample(),
				m_curCompLookahead.Sample()
			);
			
			m_compressor.Apply(sampleL, sampleR);

			m_lowCutFilter.Apply(sampleL, sampleR);

			const float gain = dBToGain(m_curMasterVol.Sample());
			sampleL *= gain;
			sampleR *= gain;

			pLeftOut[iSample]  = sampleL;
			pRightOut[iSample] = sampleR;
		}

		
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

		m_chorusOrPhaser = isChorus;
	}

	void PostPass::ApplyChorus(float sampleL, float sampleR, float &outL, float &outR, float wetness)
	{
		// Sweep modulation LFO
		const float sweepMod = fast_cosf(float(m_chorusSweepMod.Sample()));
		
		// Sweep LFOs
		const float phase = m_chorusSweep.Sample();
		
		const float sweepL = 0.5f*fast_sinf(phase+sweepMod);
		const float sweepR = 0.5f*fast_sinf((1.f-phase)+sweepMod);
		
		// 2 sweeping samples (FIXME: parametrize, though it's not a priority)
		const float delay  = m_sampleRate*0.005f; // 50ms delay
		const float spread = m_sampleRate*0.003f; // Sweep 30ms
		
		SFM_ASSERT(delay  < m_chorusDL.size());
		SFM_ASSERT(spread < m_chorusDL.size());
		
		// Take sweeped L/R samples (lowpassed to circumvent artifacts)
		const float chorusL = m_chorusDL.Read(delay + spread*m_chorusSweepLPF1.Apply(sweepL));
		const float chorusR = m_chorusDL.Read(delay + spread*m_chorusSweepLPF2.Apply(sweepR));
		
		// Mix result with dry signal
		const float maxWet = dBToGain(kMaxCPWetdB); // FIXME
		const float effectWet = wetness*maxWet;
		outL = sampleL + chorusL*effectWet; 
		outR = sampleR + chorusR*effectWet; 
	}

	void PostPass::ApplyPhaser(float sampleL, float sampleR, float &outL, float &outR, float wetness)
	{
		// Sweep LFO (lowpassed for pleasing effect)
		const float sweepMod = m_phaserSweepLPF.Apply(oscSine(m_phaserSweep.Sample()));
		
		// Sweep cutoff frequency around center
		constexpr float rangeMul = 0.25f;
		SFM_ASSERT(rangeMul <= 0.5f);
		const float range = m_Nyquist*rangeMul;
		const float cutoffCentre = m_Nyquist*0.5f + range*sweepMod;
		
		// Start with dry sample
		float filteredL = sampleL;
		float filteredR = sampleR;

		// Cutoff
		float curCutoff = cutoffCentre - range*0.5f;
		const float cutStep = range/kNumPhaserStages;
		
		// Apply cascading filters
		double curReso = 0.025;
		for (auto &filter : m_allpassFilters)
		{
			// FIXME: replace for simple 6dB filter?
			filter.updateAllpassCoeff(curCutoff, curReso, m_sampleRate);
			filter.tick(filteredL, filteredR);

			curCutoff += cutStep;		
			curReso *= 2.0;
		}
		
		// Mix result with dry signal
		const float maxWet = dBToGain(kMaxCPWetdB);
		wetness *= maxWet;
		outL = sampleL + wetness*filteredL;
		outR = sampleR + wetness*filteredR;
	}
}
