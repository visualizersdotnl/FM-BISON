
/*
	FM. BISON hybrid FM synthesis -- FM voice render (stereo).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-voice.h"
#include "synth-distort.h"

namespace SFM
{
	void Voice::ResetOperators(unsigned sampleRate)
	{
		// NULL operators
		for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
		{
			m_operators[iOp].Reset(sampleRate);
		}
	}

	// Full reset
	void Voice::Reset(unsigned sampleRate)
	{
		ResetOperators(sampleRate);

		// Not bound, zero frequency, zero velocity
		m_key = -1;
		m_velocity = 0.f;

		// Disable
		m_state = kIdle;
		m_sustained = false;

		// LFO
		m_LFO = Oscillator(sampleRate);

		// Filter envelope
		m_filterEnvelope.Reset();

		// Reset filter
		m_filterSVF1.resetState();
		m_filterSVF2.resetState();

		// Def. glide
		freqGlide = kDefPolyFreqGlide;
	}

	bool Voice::IsDone() /* const */
	{
		if (kIdle != m_state)
		{
			for (auto &voiceOp : m_operators)
			{
				if (true == voiceOp.enabled && true == voiceOp.isCarrier)
				{
					// Carrier operators should never be infinite!
					SFM_ASSERT(false == voiceOp.envelope.IsInfinite());

					// Has the envelope ran it's course yet?
					if (false == voiceOp.envelope.IsIdle())
						return false;
				}
			}
		}

		return true;
	}

	void Voice::OnRelease()
	{
		SFM_ASSERT(kPlaying == m_state);

		m_filterEnvelope.Stop();
		m_pitchEnvelope.Stop();

		for (auto &voiceOp : m_operators)
		{
			if (true == voiceOp.enabled)
			{
				voiceOp.envelope.Stop();
			}
		}

		m_state = kReleasing;
	}

	float Voice::GetSummedOutput()
	{
		float summed = 0.f;
		unsigned numCarriers = 0;

		for (auto &voiceOp : m_operators)
		{
			if (true == voiceOp.enabled && true == voiceOp.isCarrier)
			{
				summed += voiceOp.envelope.Get();
				++numCarriers;
			}
		}

		return summed;
	}

	
	/* ----------------------------------------------------------------------------------------------------

		Voice render loop, this is the most essential part of the FM engine.
		
		FIXME:
			- Rewrite so all operators can modulate each other
			- And to make it *faster*
			
	 ------------------------------------------------------------------------------------------------------ */

	 // Between [0..75] the sinus and triangle stay band limited; using feedback on a square, saw et cetera you're just *looking* for dirt!
	const float kFeedbackScale = 0.75f;

	void Voice::Sample(float &left, float &right, float pitchBend, float ampBend, float modulation)
	{
		if (kIdle == m_state)
		{
			left = right = 0.f;
			return;
		}

		SFM_ASSERT(ampBend >= 0.f && ampBend <= 2.f);
		SFM_ASSERT(modulation >= 0.f && modulation <= 1.f);

		// Sample LFO
		const float LFO = m_LFO.Sample(modulation);

		// Sample pitch envelope (does not sustain!)
		const float pitchEnv = m_pitchEnvelope.Sample(false);

		// Process all operators top-down
		// - This is a simple readable loop for R&D purposes, but lacks performance and lacks support for operators to be modulated
		//   by lower ranked operators; an issue in the R&D repository has been created to address this

		alignas(16) float opSample[kNumOperators+1] = { 0.f }; // Monaural normalized samples (one extra so we can index with -1 to avoid branching)
		float mixL = 0.f, mixR = 0.f; // Carrier mix

		for (int iOp = kNumOperators-1; iOp >= 0; --iOp)
		{
			// Top-down
			Operator &voiceOp = m_operators[iOp];
			const int iOpSample = iOp+1; // See decl. of opSample[]

			if (true == voiceOp.enabled)
			{
				// Set base freq.
				auto &oscillator = voiceOp.oscillator;
				
				const float prevFreq = voiceOp.curFreq.Get();
				const float curFreq  = voiceOp.curFreq.Sample();
				if (prevFreq != curFreq)
					oscillator.SetFrequency(curFreq);

				// Get modulation from max. 3 sources
				float phaseMod = 0.f;
				for (unsigned iMod = 0; iMod < 3; ++iMod)
				{
					const int iModulator = voiceOp.modulators[iMod];

					// Sanity checks
					SFM_ASSERT(-1 == iModulator || iModulator < kNumOperators);
					SFM_ASSERT(-1 == iModulator || iModulator > iOp);

					// Add sample to phase
					// If modulator or modulator operator is disabled it's zero
					const float sample = opSample[iModulator+1];
					phaseMod += sample; // FIXME: output -> index curve?
				}

				const float feedbackAmt = kFeedbackScale*voiceOp.feedbackAmt.Sample();

				// Get feedback
				float feedback = 0.f;
				if (-1 != voiceOp.iFeedback)
				{
					const int iFeedback = voiceOp.iFeedback;

					// Sanity check
					SFM_ASSERT(iFeedback < kNumOperators);
					
					// Grab operator's current feedback
					feedback = m_operators[iFeedback].feedbackAccum;
				}

				// Apply pitch bend, LFO vibrato & pitch envelope
				float vibrato = pitchBend; // Passed in as multiplier
				vibrato *= powf(2.f, LFO*modulation*voiceOp.pitchMod + pitchEnv);

				// Bend oscillator pitch accordingly
				oscillator.PitchBend(vibrato);

				// Calculate sample
//				float sample = oscillator.Sample(fmodf(phaseMod+feedback, 1.f));
				float sample = oscillator.Sample(phaseMod+feedback);

				// Apply LFO tremolo
				const float tremolo = lerpf<float>(1.f, LFO, voiceOp.ampMod);
				sample = lerpf<float>(sample, sample*tremolo, modulation);

				// Apply amplitude (or 'index')
				sample *= voiceOp.amplitude.Sample();

				// Apply envelope
				const float envelope = voiceOp.envelope.Sample();
				sample *= envelope;

				// Update feedback accumulator
				voiceOp.feedbackAccum = 0.25f*(voiceOp.feedbackAccum*0.995f + sample*feedbackAmt);

				// Apply distortion 
				const float driveAmt = voiceOp.drive.Sample();
				if (0.f != driveAmt)
				{
					const float squared = Squarepusher(sample, driveAmt);
					sample = lerpf<float>(sample, squared, driveAmt);
				}
				
				// Apply filter
				if (SvfLinearTrapOptimised2::NO_FLT_TYPE != voiceOp.filterSVF.getFilterType())
					voiceOp.filterSVF.tickMono(sample);

				// Store final sample for modulation
				opSample[iOpSample] = sample;

				// Calc. panning angle
				float panAngle = voiceOp.panAngle.Sample();
				const float panMod = voiceOp.panMod;
				if (0.f != panMod)
				{
					const float dryPanning = (panAngle-0.125f)*8.f;
					const float panning = Clamp(LFO*panMod*modulation + dryPanning*0.33f);
					panAngle = (panning+1.f)*0.5f*0.25f;
				}

				// If carrier, mix (chose not to branch on purpose, though it'll probably won't matter or just a little)
				sample *= voiceOp.isCarrier;
				const float monaural = sample*ampBend;
				mixL += monaural*fast_cosf(panAngle);
				mixR += monaural*fast_sinf(panAngle);
			}
		} 

		left  = mixL;
		right = mixR;
	}
}
