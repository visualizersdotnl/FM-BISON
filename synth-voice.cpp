
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
		m_LFO1   = Oscillator(sampleRate);
		m_LFO2   = Oscillator(sampleRate);
		m_modLFO = Oscillator(sampleRate);

		// Filter envelope
		m_filterEnvelope.Reset();

		// Pitch (envelope)
		m_pitchBendRange = kDefPitchBendRange;
		m_pitchEnvelope.Reset(sampleRate);

		// Reset filter
		m_filterSVF1.resetState();
		m_filterSVF2.resetState();

		// Def. glide
		m_freqGlide = kDefPolyFreqGlide;
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
			- And make it *faster*
			
	 ------------------------------------------------------------------------------------------------------ */

	 // Tame
//	constexpr float kFeedbackScale = 0.75f;
	
	// Bright
	constexpr float kFeedbackScale = 1.f;

	void Voice::Sample(float &left, float &right, float pitchBend, float ampBend, float modulation, float LFOBlend, float LFOModDepth)
	{
		if (kIdle == m_state)
		{
			SFM_ASSERT(false);

			left = right = 0.f;

			return;
		}
		
		// FIXME: why is this out of range whilst 'ampBend' isn't?
//		SFM_ASSERT(pitchBend >= 0.f && pitchBend <= 1.f);

		SFM_ASSERT(ampBend >= 0.f && ampBend <= 2.f);
		SFM_ASSERT(modulation >= 0.f && modulation <= 1.f);
		SFM_ASSERT(LFOBlend >= 0.f && LFOBlend <= 1.f);
		SFM_ASSERT(LFOModDepth >= 0.f);
		
		// Calculate LFO value
		const float modLFO = m_modLFO.Sample(0.f); // Multiply by LFOModDepth? (FIXME)

		auto modulate = [](float input, float modulation, float depth)
		{
			float sample = input*modulation;
			return Clamp(lerpf<float>(input, sample, depth));
		};

		const float LFO1 = modulate(m_LFO1.Sample(0.f), modLFO, LFOModDepth);
		const float LFO2 = modulate(m_LFO2.Sample(0.f), modLFO, LFOModDepth);
		const float blend = lerpf<float>(LFO1, LFO2, LFOBlend);

		const float LFO = Clamp(blend); // FIXME: disable Clamp() and solve issue

		// Calc. pitch envelope & bend multipliers
		const float pitchRangeOct = m_pitchBendRange/12.f;
		const float pitchEnv = powf(2.f, m_pitchEnvelope.Sample(false)*pitchRangeOct); // Sample pitch envelope (does not sustain!)
		pitchBend = powf(2.f, pitchBend*pitchRangeOct);

		// Process all operators top-down
		// It's quite verbose and algorithmically not as flexible as could be (FIXME?)

		alignas(16) float modSamples[kNumOperators] = { 0.f }; // Samples for modulation
		float mixL = 0.f, mixR = 0.f; // Carrier mix

		for (int iOp = kNumOperators-1; iOp >= 0; --iOp)
		{
			// Top-down
			Operator &voiceOp = m_operators[iOp];

			if (true == voiceOp.enabled)
			{
				// Set base freq.
				auto &oscillator = voiceOp.oscillator;
				const float curFreq = voiceOp.curFreq.Sample();
				oscillator.SetFrequency(curFreq);

				// Get modulation from max. 3 sources
				float phaseShift = 0.f;
				for (unsigned iMod = 0; iMod < 3; ++iMod)
				{
					const int iModulator = voiceOp.modulators[iMod];
					if (-1 != iModulator) // FIXME: I can do without this
					{
						// Sanity checks
						SFM_ASSERT(iModulator < kNumOperators);
						SFM_ASSERT(iModulator > iOp);

						// Add sample to phase
						// If modulator or modulator operator is disabled it's zero
						const float sample = modSamples[iModulator];
						phaseShift += 1.f+sample;
					}
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
					feedback = m_operators[iFeedback].feedback;
				}

				// Vibrato: pitch bend, pitch envelope & pitch LFO
				const float pitchLFO = powf(2.f, LFO*voiceOp.pitchMod*modulation * pitchRangeOct);
				const float vibrato = pitchBend*pitchEnv*pitchLFO;
				oscillator.PitchBend(vibrato);

				// Calculate sample
				float sample = oscillator.Sample(phaseShift+feedback);

				// LFO tremolo
				const float tremolo = fabsf(LFO*modulation);

				// Apply amplitude (or 'index')
				const float amplitude = voiceOp.amplitude.Sample();
				sample = lerpf<float>(sample*amplitude, sample*amplitude*tremolo, voiceOp.ampMod);

				// Apply envelope
				const float envelope = voiceOp.envelope.Sample();
				sample *= envelope;

				// Update operator's feedback
				voiceOp.feedback = 0.25f*(voiceOp.feedback*0.995f + sample*feedbackAmt);

				// Apply distortion 
				const float driveAmt = voiceOp.drive.Sample();
				if (0.f != driveAmt)
				{
					const float squared = Squarepusher(sample, driveAmt);
					sample = lerpf<float>(sample, squared, driveAmt);
				}
				
				// Apply filter
				bool hasOpFilter = true;
				switch (voiceOp.filters[0].getFilterType())
				{
				case SvfLinearTrapOptimised2::NO_FLT_TYPE:
					hasOpFilter = false;
					break;

				case SvfLinearTrapOptimised2::ALL_PASS_FILTER:
					for (unsigned iAllpass = 0; iAllpass < kNumVoiceAllpasses; ++iAllpass)
						voiceOp.filters[iAllpass].tickMono(sample);

					break;

				default:
					// I'm assuming the filter is set up properly
					voiceOp.filters[0].tickMono(sample);
				}

				// Store (filtered) sample for modulation
				float modSample = sample;
				
				if (false == hasOpFilter && SvfLinearTrapOptimised2::NO_FLT_TYPE != voiceOp.modFilter.getFilterType())
				{
					// Only apply if no operator filter applied and modulator filter set
					voiceOp.modFilter.tickMono(modSample);
				}

				modSamples[iOp] = modSample;

				// Add sample to gain envelope (for VU meter)
				voiceOp.envGain.Apply(fabsf(modSample));
				
				// Calculate panning
				float panning = voiceOp.panning.Sample();
				const float panMod = voiceOp.panMod;
				if (0.f != panMod && modulation > 0.f)
				{
					float modPanning = LFO*panMod*modulation*0.5f + 0.5f;

					if (panning < 0.5f)		
					{
						// Left bias
						modPanning = std::max<float>(0.f, modPanning - (0.5f-panning));
					}
					else if (panning > 0.5f)
					{
						// Right bias
						modPanning = std::min<float>(1.f, modPanning + (panning-0.5f)); 
					}

					panning = modPanning;
				}

				// If carrier, mix (chose not to branch on purpose, though it'll probably won't matter or just a little)
				sample *= voiceOp.isCarrier;
				const float monaural = sample*ampBend;
				mixL += monaural*sqrtf(1.f-panning);
				mixR += monaural*sqrtf(panning);
			}
		} 

		left  = mixL;
		right = mixR;
	}
}
