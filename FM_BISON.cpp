
/*
	FM. BISON hybrid FM synthesis
	(C) visualizers.nl & bipolar audio
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

// Shut it, MSVC
#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#include "FM_BISON.h"

#include "synth-global.h"
#include "patch/synth-patch-global.h"
#include "synth-DX7-LFO-table.h"

namespace SFM
{
	static bool s_performStaticInit = true;

	/* ----------------------------------------------------------------------------------------------------

		Constructor/Destructor

	 ------------------------------------------------------------------------------------------------------ */
	
	Bison::Bison()
	{
		if (true == s_performStaticInit)
		{
			// Calculate LUTs & initialize random generator
			InitializeRandomGenerator();
			CalculateMIDIToFrequencyLUT();
			InitializeFastCosine();

			s_performStaticInit = false;
		}
		
		// Reset entire patch
		m_patch.ResetToEngineDefaults();

		// Initialize polyphony
		m_curPolyphony = m_patch.maxVoices;

		Log("Instance of FM. BISON engine initalized");
		Log("Suzie, call DR. BISON, tell him it's for me...");

		/*
			IMPORTANT: call SetSamplingProperties() before Render()
		*/
	}

	Bison::~Bison() 
	{
		DeleteRateDependentObjects();

		Log("Instance of FM. BISON engine released");
	}

	/* ----------------------------------------------------------------------------------------------------

		OnSetSamplingProperties() is called by JUCE, but what it does is set up the basic properties
		necessary to start rendering so it can be used in any situation

	 ------------------------------------------------------------------------------------------------------ */

	// Called by JUCE's prepareToPlay()
	void Bison::OnSetSamplingProperties(unsigned sampleRate, unsigned samplesPerBlock)
	{
		Log("BISON::OnSetSamplingProperties(" + std::to_string(sampleRate) + ", " + std::to_string(samplesPerBlock) + ")");

		m_sampleRate       = sampleRate;
		m_samplesPerBlock  = samplesPerBlock;

		m_Nyquist = sampleRate>>1;

		/* 
			Reset sample rate dependent global objects
		*/

		DeleteRateDependentObjects();

		// Stop & reset all voices, clear slots & wipe requests
		for (unsigned iVoice = 0; iVoice < kMaxVoices; ++iVoice)		
			m_voices[iVoice].Reset(m_sampleRate);

		for (unsigned iSlot = 0; iSlot < 128; ++iSlot)
			m_keyToVoice[iSlot] = -1;

		m_voiceReq.clear();
		m_voiceReleaseReq.clear();

		m_resetVoices = false;

		// Reset BPM
		m_BPM = 0.0;
		m_resetPhaseBPM = true;

		// Voice mode
		m_curVoiceMode = m_patch.voiceMode;

		// Reset monophonic state
		m_monoReq.clear();

		// Reset filter type
		m_curFilterType = SvfLinearTrapOptimised2::NO_FLT_TYPE;

		// Allocate intermediate buffers (a pair for each thread)
		m_pBufL[0] = reinterpret_cast<float *>(mallocAligned(m_samplesPerBlock*sizeof(float), 16));
		m_pBufL[1] = reinterpret_cast<float *>(mallocAligned(m_samplesPerBlock*sizeof(float), 16));
		m_pBufR[0] = reinterpret_cast<float *>(mallocAligned(m_samplesPerBlock*sizeof(float), 16));
		m_pBufR[1] = reinterpret_cast<float *>(mallocAligned(m_samplesPerBlock*sizeof(float), 16));

		// Create effects
		m_postPass = new PostPass(m_sampleRate, m_samplesPerBlock, m_Nyquist);

		// Start global LFO phase
		m_globalLFO = new Phase(m_sampleRate);
		const float freqLFO = MIDI_To_DX7_LFO_Hz(m_patch.LFORate);
		m_globalLFO->Initialize(freqLFO, m_sampleRate);

		// Reset global interpolated parameters
		m_curLFOBlend    = { m_patch.LFOBlend, m_sampleRate, kDefParameterLatency };
		m_curLFOModDepth = { m_patch.LFOModDepth, m_sampleRate, kDefParameterLatency };
		m_curCutoff      = { SVF_CutoffToHz(m_patch.cutoff, m_Nyquist), m_samplesPerBlock, kDefParameterLatency * 2.f /* Longer */ };
		m_curQ           = { SVF_ResoToQ(m_patch.resonance), m_sampleRate, kDefParameterLatency };
		m_curPitchBend   = { 0.f, m_samplesPerBlock, kDefParameterLatency };
		m_curAmpBend     = { 1.f /* 0dB */, m_samplesPerBlock, kDefParameterLatency };
		m_curModulation  = { 0.f, m_samplesPerBlock, kDefParameterLatency * 1.5f /* Longer */ };
		m_curAftertouch  = { 0.f, m_samplesPerBlock, kDefParameterLatency * 3.f  /* Longer */ };

		/*
			Reset parameter (slew) filters; they reduce automation/MIDI noise (by a default slew in MS, mostly)

			They are kept *only* in this class since they're not a pretty sight and may need to be
			removed or overhauled for other (non) VST projects

			Some have a longer slew MS because they're more prone to artifacts, usually in conjuction with
			a longer parameter latency for their InterpolatedParameter counterpart

			This of course causes the response of certain parameters to be slower (or shorter) than
			the default settings, but for now I regard that simply as the instrument's behaviour
		*/

		// Their sample rate is the amount of Render() calls it takes to render a second of audio
		// FIXME: this could be a class member so it doesn't need to be passed on to ParameterSlew!
		const unsigned sampleRatePS = m_sampleRate/m_samplesPerBlock;

		// Global
		m_LFORatePS             = { sampleRatePS };
		m_LFOBlendPS            = { sampleRatePS };
		m_LFOModDepthPS         = { sampleRatePS };
		m_SandHSlewRatePS       = { sampleRatePS };
		m_cutoffPS              = { sampleRatePS, 100.f /* 100MS */ };
		m_resoPS                = { sampleRatePS };

		m_LFORatePS.Reset(freqLFO);
		m_LFOBlendPS.Reset(m_patch.LFOBlend);
		m_LFOModDepthPS.Reset(m_patch.LFOModDepth);
		m_SandHSlewRatePS.Reset(m_patch.SandHSlewRate);
		m_cutoffPS.Reset(m_patch.cutoff);
		m_resoPS.Reset(m_patch.resonance);

		// PostPass
		m_effectWetPS           = { sampleRatePS }; 
		m_effectRatePS          = { sampleRatePS };
		m_delayPS               = { sampleRatePS, 100.f /* 100MS */ };
		m_delayWetPS            = { sampleRatePS };
		m_delayFeedbackPS       = { sampleRatePS };
		m_delayFeedbackCutoffPS = { sampleRatePS };
		m_delayTapeWowPS        = { sampleRatePS };
		
		// These apply to effects operating on oversampled data, but that's not relevant to ParameterSlew
		m_tubeDistPS            = { sampleRatePS };
		m_tubeDrivePS           = { sampleRatePS };
		m_postCutoffPS          = { sampleRatePS, 100.f /* 100MS */ };
		m_postResoPS            = { sampleRatePS };
		m_postDrivePS           = { sampleRatePS };
		m_postWetPS             = { sampleRatePS };
		
		m_wahRatePS              = { sampleRatePS };
		m_wahDrivePS             = { sampleRatePS };
		m_wahSpeakPS             = { sampleRatePS };
		m_wahSpeakVowelPS        = { sampleRatePS, 10.f /* 10MS */ };
		m_wahSpeakVowelModPS     = { sampleRatePS };
		m_wahSpeakGhostPS        = { sampleRatePS };
		m_wahSpeakCutPS          = { sampleRatePS, 100.f /* 100MS */ };
		m_wahSpeakResoPS         = { sampleRatePS, 10.f  /*  10MS */ };
		m_wahCutPS               = { sampleRatePS };
		m_wahWetPS               = { sampleRatePS };
		m_reverbWetPS            = { sampleRatePS };
		m_reverbRoomSizePS       = { sampleRatePS };
		m_reverbDampeningPS      = { sampleRatePS };
		m_reverbWidthPS          = { sampleRatePS };
		m_reverbBassTuningdBPS   = { sampleRatePS };
		m_reverbTrebleTuningdBPS = { sampleRatePS };
		m_reverbPreDelayPS       = { sampleRatePS, 100.f /* 100MS */ };
		m_compLookaheadPS        = { sampleRatePS, 100.f /* 100MS */ };
		m_masterVoldBPS          = { sampleRatePS };
		m_bassTuningdBPS         = { sampleRatePS };
		m_trebleTuningdBPS       = { sampleRatePS };
		m_midTuningdBPS          = { sampleRatePS };
		
		// FIXME: move to constructors above
		m_effectWetPS.Reset(m_patch.cpWet);
		m_effectRatePS.Reset(m_patch.cpRate);
		m_delayPS.Reset(m_patch.delayInSec);
		m_delayWetPS.Reset(m_patch.delayWet);
		m_delayFeedbackPS.Reset(m_patch.delayFeedback);
		m_delayFeedbackCutoffPS.Reset(m_patch.delayFeedbackCutoff);
		m_delayTapeWowPS.Reset(m_patch.delayTapeWow);
		m_postCutoffPS.Reset(m_patch.postCutoff);
		m_postResoPS.Reset(m_patch.postResonance);
		m_postDrivePS.Reset(m_patch.postDrivedB);
		m_postWetPS.Reset(m_patch.postWet);
		m_tubeDistPS.Reset(m_patch.tubeDistort);
		m_tubeDrivePS.Reset(m_patch.tubeDrive);
		m_wahRatePS.Reset(m_patch.wahRate);
		m_wahDrivePS.Reset(m_patch.wahDrivedB);
		m_wahSpeakPS.Reset(m_patch.wahSpeak);
		m_wahSpeakVowelPS.Reset(m_patch.wahSpeakVowel);
		m_wahSpeakVowelModPS.Reset(m_patch.wahSpeakVowelMod);
		m_wahSpeakGhostPS.Reset(m_patch.wahSpeakGhost);
		m_wahSpeakCutPS.Reset(m_patch.wahSpeakCut);
		m_wahSpeakResoPS.Reset(m_patch.wahSpeakResonance);
		m_wahCutPS.Reset(m_patch.wahCut);
		m_wahWetPS.Reset(m_patch.wahWet);
		m_reverbWetPS.Reset(m_patch.reverbWet);
		m_reverbRoomSizePS.Reset(m_patch.reverbRoomSize);
		m_reverbDampeningPS.Reset(m_patch.reverbDampening);
		m_reverbWidthPS.Reset(m_patch.reverbWidth);
		m_reverbBassTuningdBPS.Reset(m_patch.reverbBassTuningdB);
		m_reverbTrebleTuningdBPS.Reset(m_patch.reverbTrebleTuningdB);
		m_reverbPreDelayPS.Reset(m_patch.reverbPreDelay);
		m_compLookaheadPS.Reset(0.f);
		m_masterVoldBPS.Reset(m_patch.masterVoldB);
		m_bassTuningdBPS.Reset(m_patch.bassTuningdB);
		m_trebleTuningdBPS.Reset(m_patch.trebleTuningdB);
		m_midTuningdBPS.Reset(m_patch.midTuningdB);

		// Auto-wah
		m_autoWahPedalPS = { sampleRatePS, kDefParameterSlewMS*0.5f /* A bit faster */ };
		m_autoWahPedalPS.Reset(1.f);

		// Local
		m_bendWheelPS  = { sampleRatePS };
		m_modulationPS = { sampleRatePS };
		m_aftertouchPS = { sampleRatePS };

		// Reset operator peak followers
		for (auto &peakEnv : m_opPeaksEnv)
		{
			peakEnv.SetTimeCoeff(1.f); // 1MS
			peakEnv.SetSampleRate(sampleRate/samplesPerBlock); // Updated once per Render() call
		}

		for (unsigned iPeak = 0; iPeak < kNumOperators; ++iPeak)
			m_opPeaks[iPeak] = 0.f;
	}

	// Cleans up after OnSetSamplingProperties()
	void Bison::DeleteRateDependentObjects()
	{
		// Allocate intermediate sample buffers
		freeAligned(m_pBufL[0]);
		freeAligned(m_pBufL[1]);
		freeAligned(m_pBufR[0]);
		freeAligned(m_pBufR[1]);
		m_pBufL[0] = m_pBufL[1] = m_pBufR[0] = m_pBufR[1] = nullptr;

		// Release post-pass
		delete m_postPass;
		m_postPass = nullptr;

		// Delete global LFO
		delete m_globalLFO;
		m_globalLFO = nullptr;
	}

	/* ----------------------------------------------------------------------------------------------------

		Voice management

		Polyphonic mode works as you'd expect

		Monophonic mode:
		- Each note uses it's own velocity for all calculations, instead of carrying over the
		  initial one (like I can hear my Yamaha TG77 do)
		- When a note is released, the last note in the sequence will play; to restart lift all keys
		- Glide speed and velocity attenuation can be controlled through parameters

	 ------------------------------------------------------------------------------------------------------ */

	// Release voice, but retain key slot
	void Bison::ReleaseVoice(int index)
	{
		SFM_ASSERT(index >= 0 && index < kMaxVoices);

		Voice &voice = m_voices[index];
		SFM_ASSERT(true == voice.IsPlaying());
		
		// Lift possible sustain & release
		voice.m_sustained = false;
		voice.OnRelease();

		const int key = voice.m_key;
		Log("Voice released: " + std::to_string(index) + " for key: " + std::to_string(key));
	}
	
	// Free voice & key slot immediately
	void Bison::FreeVoice(int index)
	{
		
		
		SFM_ASSERT(index >= 0 && index < kMaxVoices);

		Voice &voice = m_voices[index];

		SFM_ASSERT(false == voice.IsIdle());

		// Set to idle, lift possible sustain
		voice.m_state = Voice::kIdle;
		voice.m_sustained = false;

		// Decrease global count
		SFM_ASSERT(m_voiceCount > 0);
		--m_voiceCount;
		
		// Free key
		const int key = voice.m_key;
		if (-1 != key)
		{
			FreeKey(voice.m_key);
			voice.m_key = -1;

			Log("Voice freed: " + std::to_string(index) + " for key: " + std::to_string(key));
		}
		else
			Log("Voice freed: " + std::to_string(index));
	}

	// Steal voice (quick fade)
	void Bison::StealVoice(int index)
	{
		SFM_ASSERT(index >= 0 && index < kMaxVoices);

		Voice &voice = m_voices[index];
		SFM_ASSERT(false == voice.IsIdle());

		// Can not steal what has already been stolen!
		SFM_ASSERT(false == voice.IsStolen());

		// Not active? Does not need to be stolen
		if (true == voice.IsIdle())
			return;

		// Flag as stolen so next Render() call will handle it
		voice.m_state = Voice::kStolen;

		// Free key (if not already done)
		const int key = voice.m_key;
		if (-1 != key)
		{
			FreeKey(key);
			voice.m_key = -1;

			Log("Voice stolen: " + std::to_string(index) + " for key: " + std::to_string(key));
		}
		else
			Log("Voice stolen: " + std::to_string(index));
	}

	void Bison::NoteOn(unsigned key, float frequency, float velocity, unsigned timeStamp, bool isMonoRetrigger /* = false */)
	{
		SFM_ASSERT(key <= 127);
		SFM_ASSERT(velocity >= 0.f && velocity <= 1.f);

		// In case of duplicates honour the first NOTE_ON
		for (const auto &request : m_voiceReq)
			if (request.key == key)
			{
				Log("Duplicate NoteOn() for key: " + std::to_string(key));
				return;
			}

		VoiceRequest request;
		request.key           = key;
		request.frequency     = frequency;
		request.velocity      = velocity;
		request.timeStamp     = timeStamp;
		request.monoRetrigger = isMonoRetrigger;
		
		const bool monophonic = Patch::VoiceMode::kMono == m_patch.voiceMode;

		const int index = GetVoice(key);

		if (false == monophonic)
		{
			/* Polyphonic */
			
			// Key bound to voice?
			if (index >= 0)
			{
				Voice &voice = m_voices[index];

				if (false == voice.IsIdle())
				{
					if (true == monophonic)
					{
						/*
							FIXME: I've kept this logic for monophonic mode, but purely because I did not want to mess with it!
						*/

						// Release if still playing
						if (true == voice.IsPlaying())
							ReleaseVoice(index);

						// Disassociate voice from key
						FreeKey(voice.m_key);
						voice.m_key = -1;
					}
					else
					{
						// Steal if still playing (performance fix)
						if (false == voice.IsStolen())
							StealVoice(index);
					}

					Log("NoteOn() retrigger: " + std::to_string(key) + ", voice: " + std::to_string(index));
				}
			}

			// Issue request
			if (m_voiceReq.size() < m_curPolyphony)
			{
				m_voiceReq.emplace_back(request);
			}
			else
			{
				// Replace last request (FIXME: replace by time stamp instead?)
				m_voiceReq.pop_back();
				m_voiceReq.emplace_back(request);
			}
		}
		else
		{
			/* Monophonic */

			SFM_ASSERT(1 == m_curPolyphony);

			// Issue first request only
			if (m_voiceReq.size() < m_curPolyphony)
			{
				m_voiceReq.emplace_back(request);

				// Add to sequence
				m_monoReq.emplace_front(request);
			}
		}
	}

	void Bison::NoteOff(unsigned key, unsigned timeStamp)
	{
		SFM_ASSERT(key <= 127);

		// In case of duplicates honour the first NOTE_OFF
		for (auto request : m_voiceReleaseReq)
			if (request == key)
			{
				Log("Duplicate NoteOff() for key: " + std::to_string(key));
				return;
			}

		const bool monophonic = Patch::VoiceMode::kMono == m_patch.voiceMode;

		const int index = GetVoice(key);
		if (index >= 0)
		{
			if (false == monophonic)
			{
				// Issue request				
				m_voiceReleaseReq.push_back(key);
			}
			else
			{
				// Monophonic: issue only 1 (the last) request
				m_voiceReleaseReq.clear();
				m_voiceReleaseReq.push_back(key);

				Log("Release issued for monophonic key: " + std::to_string(key));
			}
		}
		
		// It might be that a deferred request matches this NOTE_OFF, in which case we get rid of the request
		for (auto iReq = m_voiceReq.begin(); iReq != m_voiceReq.end(); ++iReq)
		{
			if (iReq->key == key)
			{
				// Erase and break since NoteOn() ensures there are no duplicates in the deque
				m_voiceReq.erase(iReq);

				Log("Deferred NoteOn() removed due to matching NOTE_OFF for key: " + std::to_string(key));

				break;
			}
		}

		if (true == monophonic)
		{
			/* Monophonic */

			// Remove key from sequence
			for (auto iReq = m_monoReq.begin(); iReq != m_monoReq.end(); ++iReq)
			{
				if (iReq->key == key)
				{
					m_monoReq.erase(iReq);

					Log("Removed from monophonic sequence, key: " + std::to_string(key) + ", deque size: " + std::to_string(m_monoReq.size()));

					break;
				}
			}
			
			// Request in deque?
			if (true == m_voiceReq.empty() && false == m_monoReq.empty())
			{
				/* const */ auto &voice = m_voices[0];

				float output = 0.f;
				if (false == voice.IsIdle())
					output = voice.GetSummedOutput();

				const bool isSilent = output == 0.f;

				if (false == isSilent) // If silent, wait for player to start a new sequence
				{
					// Retrigger frontmost note in sequence
					const auto &request = m_monoReq.front();
					NoteOn(request.key, request.frequency, request.velocity, timeStamp, true);
					m_monoReq.pop_front();

					Log("Mono NOTE_ON for prev. key " +  std::to_string(request.key));
				}
				else
				{
					// If we fell silent, clear the deque as we'll be starting a new sequence
					m_monoReq.clear();

					Log("Mono seq. fell silent, erasing request(s)");
				}
			}
		}
	}

	/* ----------------------------------------------------------------------------------------------------

		Voice initialization helper functions

	 ------------------------------------------------------------------------------------------------------ */

	// Calc. operator frequency
	static float CalcOpFreq(float fundamentalFreq, float detuneOffs, const PatchOperators::Operator &patchOp)
	{
		float frequency;
		if (true == patchOp.fixed)
		{
			frequency = (float) patchOp.coarse;
			SFM_ASSERT(frequency >= 0.f && frequency <= kMaxFixedHz);

			// Fixed requency does not necessarily have to be below Nyquist
			// Ratio, fine and detune controls are disabled in VST UI
		}
		else
		{
			frequency = fundamentalFreq;

			const int   coarse = patchOp.coarse;              // Ratio
			const int     fine = patchOp.fine;                // Semitones
			const float detune = patchOp.detune + detuneOffs; // Cents

			SFM_ASSERT(coarse >= kCoarseMin && coarse <= kCoarseMax);
			SFM_ASSERT(abs(fine) <= kFineRange);
			SFM_ASSERT(abs(detune) <= kDetuneRange);
			
			frequency *= powf(2.f, (detune*0.01f)/12.f);

			if (coarse < 0)
				frequency /= abs(coarse-1);
			else if (coarse > 1)
				frequency *= coarse;
			
			frequency *= powf(2.f, fine/12.f);
		}

		return frequency;
	}

	// Calc. multiplier for operator amplitude or modulation index
	static float CalcOpLevel(unsigned key, float velocity, const PatchOperators::Operator &patchOp)
	{
		float multiplier = 1.f;

		// Factor in velocity
		const float velPow = velocity*velocity;
		multiplier = lerpf<float>(multiplier, multiplier*velPow, patchOp.velSens);
		
		// Apply L/R breakpoint cut & level scaling (subtractive/additive & linear/exponential, like the DX7)
		const unsigned breakpoint = patchOp.levelScaleBP;

		if (true == patchOp.cutLeftOfLSBP && true == patchOp.cutRightOfLSBP)
		{
			// Range cut
			const unsigned sides = 127-breakpoint;

			unsigned left = sides/2;
			const unsigned remainder = left % 12;
			left += 12-remainder; // Align right to high C note

			const unsigned right = left+breakpoint;
			
			if (key < left || key > right)
				multiplier = 0.f;
		}
		else if (true == patchOp.cutLeftOfLSBP && key < breakpoint)
			// Cut left
			multiplier = 0.f;
		else if (true == patchOp.cutRightOfLSBP && key > breakpoint)
			// Cut right
			multiplier = 0.f;
		
		// We didn't cut, so apply level scaling as usual
		if (0.f != multiplier)
		{
			// Apply level scaling
			const unsigned numSemis = patchOp.levelScaleRange;
			if (0 != numSemis)
			{
				const bool keyIsLeftOfBP  = key < breakpoint;
				const bool keyIsRightOfBP = key > breakpoint;

				const float levelStep = 1.f/numSemis;

				int distance = 0;
				float amount = 0.f;
				bool isExponential = false;

				if (true == keyIsLeftOfBP)
				{
					distance = breakpoint-key;
					amount = patchOp.levelScaleL;
					isExponential = patchOp.levelScaleExpL;
				}
				else if (true == keyIsRightOfBP)
				{
					distance = key-breakpoint;
					amount = patchOp.levelScaleR;
					isExponential = patchOp.levelScaleExpR;
				}

				// Calculate normalized distance from BP
				distance = std::min<int>(numSemis, abs(distance));
				const float linear = smoothstepf(distance*levelStep); // This (smoothstep) takes the edges off, results in a smoother glide
				const float factor = false == isExponential ? linear : powf(linear, 1.f-linear) /* -EXP/+EXP */;

				if (amount < 0.f)
					// Fade out by gradually interpolating towards lower level
					multiplier = lerpf<float>(multiplier, multiplier*(1.f-fabsf(amount)), factor);
				else if (amount > 0.f)
					// Fade in by adding to set level
					multiplier = lerpf<float>(multiplier, std::min<float>(1.f, multiplier+fabsf(amount)), factor);

				// Subtractive as well as additive scaling leave the multiplier (output) on the other side
				// of the breakpoint intact; this makes it intuitive to use this feature (I think)
			}
		}

		SFM_ASSERT(multiplier >= 0.f && multiplier <= 1.f);
		
		// Return level multiplier!
		return multiplier;
	}

	// Simply scales [-1..1] to [0.5..0.5]
	SFM_INLINE static float CalcPanning(const PatchOperators::Operator &patchOp)
	{
		SFM_ASSERT(fabsf(patchOp.panning) <= 1.f);
		return 0.5f*patchOp.panning + 0.5f;
	}


	SFM_INLINE static float CalcPhaseJitter(float jitter)
	{
		SFM_ASSERT(jitter >= 0.f && jitter <= 1.f);
		return jitter*mt_randf()*0.25f; // [0..90] deg.
	}

	SFM_INLINE static float CalcPhaseShift(const Voice::Operator &voiceOp, const PatchOperators::Operator &patchOp)
	{
		// For certain oscillators (supersaw, noise, ...) this is useseless but not worth checking for
		const float shift = (true == patchOp.keySync)
			? 0.f // Synchronized
			: voiceOp.oscillator.GetPhase(); // No key sync.: use last known phase (works well enough for 'normal' oscillators, others run free)

		return shift;
	}

	// Returns [-1..1]
	SFM_INLINE static float CalcOpCutoffKeyTracking(unsigned key, float cutoffKeyTrack)
	{
		SFM_ASSERT(key >= 0 && key <= 127);
		const float normalizedKey = key/127.f;

		/* const */ float tracking = cutoffKeyTrack;
		SFM_ASSERT(tracking >= -1.f && tracking <= 1.f);

		return tracking*normalizedKey;
	}

	// Set up (static) operator filter
//	static void SetOperatorFilters(unsigned key, unsigned sampleRate, SvfLinearTrapOptimised2 &filter, SvfLinearTrapOptimised2 &modFilter, const PatchOperators::Operator &patchOp)
	static void SetOperatorFilters(unsigned key, unsigned sampleRate, Biquad &filter, SvfLinearTrapOptimised2 &modFilter, const PatchOperators::Operator &patchOp)
	{
		SFM_ASSERT(sampleRate > 0);

		const unsigned Nyquist = sampleRate/2;

//		filter.resetState();    //
//		filter.reset();         // <- Shouldn't be necessary, as we always initialize it or set it to 'bq_type_none'
		modFilter.resetState(); //
		
		const float normQ = patchOp.resonance;
		
		// Calculate keytracking (only for LPF & HPF)
		float tracking = -1.f, cutoffNormFrom = -1.f, cutoffNormTo = -1.f;
		
		if (PatchOperators::Operator::kLowpassFilter == patchOp.filterType || PatchOperators::Operator::kHighpassFilter == patchOp.filterType)
		{
			const float cutoffKeyTrack = patchOp.cutoffKeyTrack;
			tracking = CalcOpCutoffKeyTracking(key, cutoffKeyTrack);

			cutoffNormFrom = patchOp.cutoff;
			cutoffNormTo = (tracking >= 0.f) ? 1.f : 0.f;
		
			tracking = fabsf(tracking);
		}
		
		float cutoffNorm = -1.f; // Causes assertion if not set
		const float biQ = 0.01f + 9.99f*normQ; // See Biquad.h
		switch (patchOp.filterType)
		{
		default:
			SFM_ASSERT(false);

		case PatchOperators::Operator::kNoFilter:
			filter.setBiquad(bq_type_none, 0.f, 0.f, 0.f);
			break;

		case PatchOperators::Operator::kLowpassFilter:
			cutoffNorm = lerpf<float>(cutoffNormFrom, cutoffNormTo, tracking); // Keytrack
			filter.setBiquad(bq_type_lowpass, BQ_CutoffToHz(cutoffNorm, Nyquist)/sampleRate, biQ, 0.f);
//			filter.updateLowpassCoeff(SVF_CutoffToHz(cutoffNorm, Nyquist), SVF_ResoToQ(normQ), sampleRate);
			break;

		case PatchOperators::Operator::kHighpassFilter:
			cutoffNorm = lerpf<float>(cutoffNormFrom, 1.f-cutoffNormTo, tracking); // Keytrack
			filter.setBiquad(bq_type_highpass, BQ_CutoffToHz(cutoffNorm, Nyquist)/sampleRate, biQ, 0.f);
//			filter.updateHighpassCoeff(SVF_CutoffToHz(cutoffNorm, Nyquist), SVF_ResoToQ(normQ), sampleRate);
			break;

		case PatchOperators::Operator::kBandpassFilter:
			filter.setBiquad(bq_type_bandpass, BQ_CutoffToHz(patchOp.cutoff, Nyquist)/sampleRate, biQ, 0.f);
//			filter.updateCoefficients(SVF_CutoffToHz(patchOp.cutoff, Nyquist), SVF_ResoToQ(0.25f*normQ), SvfLinearTrapOptimised2::BAND_PASS_FILTER, sampleRate);
			break;

		case PatchOperators::Operator::kPeakFilter:
			SFM_ASSERT(patchOp.peakdB >= kMinOpFilterPeakdB && patchOp.peakdB <= kMaxOpFilterPeakdB);
			filter.setBiquad(bq_type_peak, BQ_CutoffToHz(cutoffNorm, Nyquist)/sampleRate, biQ, patchOp.peakdB);
//			filter.setGain(patchOp.peakdB);
//			filter.updateCoefficients(SVF_CutoffToHz(patchOp.cutoff, Nyquist), SVF_ResoToQ(0.25f - 0.25f*normQ /* FIXME: !? */), SvfLinearTrapOptimised2::BELL_FILTER, sampleRate);
			break;
		}
		
		switch (patchOp.waveform)
		{
			// These waveforms shall remain unaltered
			case Oscillator::kSine:
			case Oscillator::kCosine:
			case Oscillator::kPolyTriangle:
			case Oscillator::kSupersaw: // Checked and it's not necessary (though at first I assumed it would be)
				modFilter.updateNone();
				break;
			
			// Filter the remaining waveforms a little to "take the top off"
			default:
				modFilter.updateLowpassCoeff(SVF_CutoffToHz(kModulatorLP, Nyquist), kSVFLowestFilterQ, sampleRate);
				break;
		}
	}
	
	// Calc. tracking (linear or 'acoustically curved')
	SFM_INLINE static float CalcKeyTracking(unsigned key, const PatchOperators::Operator &patchOp)
	{
		SFM_ASSERT(key >= 0 && key <= 127);
		const float normalizedKey = key/127.f;

		SFM_ASSERT(patchOp.envKeyTrack >= 0.f && patchOp.envKeyTrack <= 1.f);
		
		return (false == patchOp.acousticEnvKeyTrack)
			? 1.f - 0.9f*patchOp.envKeyTrack*normalizedKey
			: AcousticTrackingCurve(normalizedKey, patchOp.envKeyTrack); // See impl. for details
	}

	// Calc. LFO frequencies
	SFM_INLINE static void CalcLFOFreq(float &frequency /* Set to base freq. */, float &modFrequency, int speedAdj)
	{
		SFM_ASSERT(frequency > 0.f);
		SFM_ASSERT(speedAdj >= kMinLFOModSpeed && speedAdj <= kMaxLFOModSpeed);

		const float freqSpeedAdj = powf(2.f, float(speedAdj));
		modFrequency = frequency*freqSpeedAdj;
	}

	// Initializes LFOs (used on top of Voice::Sample())
	void Bison::InitializeLFOs(Voice &voice, float jitter)
	{
		// Calc. shift
		float phaseShift = (true == m_patch.LFOKeySync)
			? 0.f // Synchronized 
			: m_globalLFO->Get(); // Free running

		// Add jitter
		phaseShift += CalcPhaseJitter(jitter);
		
		// Frequencies
		float frequency = m_globalLFO->GetFrequency(), modFrequency;
		CalcLFOFreq(frequency, modFrequency, m_patch.LFOModSpeed);

		// Set up LFOs
		voice.m_LFO1.Initialize(m_patch.LFOWaveform1,   frequency,    m_sampleRate, phaseShift);
		voice.m_LFO2.Initialize(m_patch.LFOWaveform2,   frequency,    m_sampleRate, phaseShift);
		voice.m_modLFO.Initialize(m_patch.LFOWaveform3, modFrequency, m_sampleRate, phaseShift);
	}

	/* ----------------------------------------------------------------------------------------------------

		Voice initialization; there's a separate function for a monophonic voice

	 ------------------------------------------------------------------------------------------------------ */
	
	// Initialize new voice
	void Bison::InitializeVoice(const VoiceRequest &request, unsigned iVoice)
	{
		Voice &voice = m_voices[iVoice];

		// No voice reset, this function should initialize all necessary components
		// and be able to use previous values such as oscillator phase to enable/disable

		// Voice not sustained
		voice.m_sustained = false;

		// Offset in samples (to be within a single Render() pass)
		SFM_ASSERT(request.timeStamp <= m_samplesPerBlock);
		voice.m_sampleOffs = request.timeStamp;

		const unsigned key = request.key;        // Key
		const float jitter = m_patch.jitter;     // Jitter
		const float velocity = request.velocity; // Velocity

		// Store key & velocity immediately (used by CalcOpLevel())
		voice.m_key = key;
		voice.m_velocity = velocity;

		// Get fundamental freq. (using JUCE-supplied freq. for now)
		float fundamentalFreq = (-1.f == request.frequency)
			? float(g_MIDIToFreqLUT[key])
			: request.frequency;

		// Note frequency jitter
		const float noteJitter = jitter*mt_randfc()*kMaxNoteJitter;
		fundamentalFreq *= powf(2.f, (noteJitter*0.01f)/12.f);

		voice.m_fundamentalFreq = fundamentalFreq;
		
		// Initialize LFO
		InitializeLFOs(voice, jitter);

		// Get dry FM patch		
		PatchOperators &patchOps = m_patch.operators;

		// Default glide (in case frequency is manipulated whilst playing)
		voice.m_freqGlide = kDefPolyFreqGlide;

		// Acoustic scaling: more velocity can mean longer envelope decay phase
		// This is specifically designed for piano, guitar et cetera: strum or strike
		// harder and the decay phase will be longer
		const float envAcousticScaling = 1.f + (velocity*velocity)*m_patch.acousticScaling;

		// Set up voice operators
		for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
		{
			const PatchOperators::Operator &patchOp = patchOps.operators[iOp];
			Voice::Operator &voiceOp = voice.m_operators[iOp];

			voiceOp.enabled   = patchOp.enabled;
			voiceOp.isCarrier = patchOp.isCarrier;

			voiceOp.envGain.Reset();

			if (true == voiceOp.enabled)
			{
				// Operator velocity
				const float opVelocity = (false == patchOp.velocityInvert) ? velocity : 1.f-velocity;

				// (Re)set constant/static filters
				SetOperatorFilters(key, m_sampleRate, voiceOp.filter, voiceOp.modFilter, patchOp);
				
				// Store detune jitter
				voiceOp.detuneOffs = jitter*mt_randfc()*patchOp.detune*kMaxDetuneJitter;
	
				const float frequency = CalcOpFreq(fundamentalFreq, voiceOp.detuneOffs, patchOp);
				
				// Get amplitude & index
				const float level = CalcOpLevel(key, opVelocity, patchOp);
				const float amplitude = patchOp.output*level, index = patchOp.output*level; /* FIXME: patchOp.index*level; */

				voiceOp.oscillator.Initialize(
					patchOp.waveform, frequency, m_sampleRate, CalcPhaseShift(voiceOp, patchOp), patchOp.supersawDetune, patchOp.supersawMix);

				// Set (static) amplitude
				voiceOp.amplitude.SetRate(m_sampleRate, kDefParameterLatency);
				voiceOp.amplitude.Set(amplitude);

				// Set (static) index
				voiceOp.index.SetRate(m_sampleRate, kDefParameterLatency);
				voiceOp.index.Set(index);
					
				// No interpolation
				voiceOp.curFreq.SetRate(m_sampleRate, kDefPolyFreqGlide);
				voiceOp.curFreq.Set(frequency);
				voiceOp.setFrequency = frequency;

				// Envelope key tracking
				const float envKeyTracking = CalcKeyTracking(key, patchOp);
				
				// Start envelope
				voiceOp.envelope.Start(patchOp.envParams, m_sampleRate, patchOp.isCarrier, envKeyTracking, envAcousticScaling); 

				// Modulation/Feedback sources
				voiceOp.modulators[0] = patchOp.modulators[0];
				voiceOp.modulators[1] = patchOp.modulators[1];
				voiceOp.modulators[2] = patchOp.modulators[2];

				// Feedback
				voiceOp.iFeedback   = patchOp.feedback;
				voiceOp.feedbackAmt = { patchOp.feedbackAmt, m_sampleRate, kDefParameterLatency };
				voiceOp.feedback    = 0.f;
				
				// LFO influence
				voiceOp.ampMod   = patchOp.ampMod;
				voiceOp.pitchMod = patchOp.pitchMod;
				voiceOp.panMod   = patchOp.panMod;

				// Panning
				voiceOp.panning = { CalcPanning(patchOp), m_sampleRate, kDefParameterLatency };

				// Distortion
				const float drive = lerpf<float>(patchOp.drive, patchOp.drive*opVelocity, patchOp.velSens);
				voiceOp.drive = { drive, m_sampleRate, kDefParameterLatency };
			}
		}

		// Reset filters
		voice.m_filterSVF1.resetState();
		voice.m_filterSVF2.resetState();

		// Start filter envelope
		voice.m_filterEnvelope.Start(m_patch.filterEnvParams, m_sampleRate, false, 1.f, envAcousticScaling);

		// Start pitch envelope
		voice.m_pitchBendRange = m_patch.pitchBendRange;
		voice.m_pitchEnvelope.Start(m_patch.pitchEnvParams, m_sampleRate);

		// Voice is now playing
		voice.m_state = Voice::kPlaying;
		++m_voiceCount;

		// Store (new) index in key slot
		SFM_ASSERT(-1 == GetVoice(key));
		SetKey(key, iVoice);
	}

	// Specialized function for monophonic voices
	// Not ideal due to some code duplication, but easier to modify and follow
	void Bison::InitializeMonoVoice(const VoiceRequest &request)
	{
		Voice &voice = m_voices[0];

		// No voice reset, this function should initialize all necessary components
		// and be able to use previous values such as oscillator phase to enable/disable

		const bool isReleasing = voice.IsReleasing();
		bool reset = voice.IsDone() || true == isReleasing;

		// Retrigger mono. sequence?
		if (true == request.monoRetrigger)
			reset = false;

		// Won't reset envelope until all keys are depressed
		if (m_monoReq.size() > 1)
			reset = false;

		// Voice not sustained
		voice.m_sustained = false;

		const unsigned key = request.key;    // Key
		const float jitter = m_patch.jitter; // Jitter

		const float velocity = request.velocity;

		// Store key & velocity immediately (used by CalcOpLevel())
		voice.m_key = key;
		voice.m_velocity = velocity;

		// Get fundamental freq. (using JUCE-supplied freq. for now)
		float fundamentalFreq = (-1.f == request.frequency)
			? float(g_MIDIToFreqLUT[key])
			: request.frequency;

		// Note frequency jitter
		const float noteJitter = jitter * (-1.f + mt_randf()*2.f);
		fundamentalFreq *= powf(2.f, (noteJitter*kMaxNoteJitter*0.01f)/12.f);

		voice.m_fundamentalFreq = fundamentalFreq;
		
		if (true == reset)
		{
			// Initialize LFO
			InitializeLFOs(voice, jitter);
		}

		// See InitializeVoice()
		const float envAcousticScaling = 1.f + (velocity*velocity)*m_patch.acousticScaling;

		// Get patch operators		
		PatchOperators &patchOps = m_patch.operators;

		// Calc. attenuated glide (using new velocity, feels more natural)
		float monoGlide = m_patch.monoGlide;
		float glideAtt = 1.f - m_patch.monoAtt*request.velocity;
		voice.m_freqGlide = monoGlide*glideAtt;

		// Set up voice operators
		for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
		{
			const PatchOperators::Operator &patchOp = patchOps.operators[iOp];
			Voice::Operator &voiceOp = voice.m_operators[iOp];

			voiceOp.enabled   = patchOp.enabled;
			voiceOp.isCarrier = patchOp.isCarrier;

			voiceOp.envGain.Reset();

			if (true == voiceOp.enabled)
			{
				// Operator velocity
				const float opVelocity = (false == patchOp.velocityInvert) ? velocity : 1.f-velocity;

				if (true == reset)
				{
					// (Re)set constant/static filters
					SetOperatorFilters(key, m_sampleRate, voiceOp.filter, voiceOp.modFilter, patchOp);
				}

				// Store detune jitter
				voiceOp.detuneOffs = jitter*mt_randfc()*patchOp.detune*kMaxDetuneJitter;
				
				const float frequency = CalcOpFreq(fundamentalFreq, voiceOp.detuneOffs, patchOp);

				// Get amplitude & index
				const float level = CalcOpLevel(key, opVelocity, patchOp);
				const float amplitude = patchOp.output*level, index = patchOp.output*level; /* FIXME: patchOp.index*level; */

				if (true == reset)
				{
					voiceOp.oscillator.Initialize(
						patchOp.waveform, frequency, m_sampleRate, CalcPhaseShift(voiceOp, patchOp), patchOp.supersawDetune, patchOp.supersawMix);

					// Set amplitude
					voiceOp.amplitude.SetRate(m_sampleRate, kDefParameterLatency);
					voiceOp.amplitude.Set(amplitude);

					// Set index
					voiceOp.index.SetRate(m_sampleRate, kDefParameterLatency);
					voiceOp.index.Set(index);

					// Set freq.
					voiceOp.curFreq.SetRate(m_sampleRate, voice.m_freqGlide);
					voiceOp.curFreq.Set(frequency);

					const float envKeyTracking = CalcKeyTracking(key, patchOp);
					voiceOp.envelope.Start(patchOp.envParams, m_sampleRate, patchOp.isCarrier, envKeyTracking, envAcousticScaling); 
				}
				else
				{
					// Glide
					voiceOp.amplitude.SetTarget(amplitude);
					voiceOp.index.SetTarget(index);
					
					const float curFreq = voiceOp.curFreq.Get();
					voiceOp.curFreq.SetRate(m_sampleRate, voice.m_freqGlide);
					voiceOp.curFreq.Set(curFreq);
					voiceOp.curFreq.SetTarget(frequency);
				}

				voiceOp.setFrequency = frequency;

				// Modulation/Feedback sources
				voiceOp.modulators[0] = patchOp.modulators[0];
				voiceOp.modulators[1] = patchOp.modulators[1];
				voiceOp.modulators[2] = patchOp.modulators[2];

				// Feedback
				voiceOp.iFeedback   = patchOp.feedback;
				voiceOp.feedbackAmt = { patchOp.feedbackAmt, m_sampleRate, kDefParameterLatency };
				voiceOp.feedback    = 0.f;
				
				// LFO influence
				voiceOp.ampMod   = patchOp.ampMod;
				voiceOp.pitchMod = patchOp.pitchMod;
				voiceOp.panMod   = patchOp.panMod;

				// Panning
				voiceOp.panning = { CalcPanning(patchOp), m_sampleRate, kDefParameterLatency };

				// Distortion
				const float drive = lerpf<float>(patchOp.drive, patchOp.drive*opVelocity, patchOp.velSens);
				voiceOp.drive = { drive, m_sampleRate, kDefParameterLatency };
			}
		}

		if (true == reset)
		{
			// Reset filters
			voice.m_filterSVF1.resetState();
			voice.m_filterSVF2.resetState();
			
			// Start filter envelope
			voice.m_filterEnvelope.Start(m_patch.filterEnvParams, m_sampleRate, false, 1.f, envAcousticScaling);

			// Start pitch envelope
			voice.m_pitchBendRange = m_patch.pitchBendRange;
			voice.m_pitchEnvelope.Start(m_patch.pitchEnvParams, m_sampleRate);
		}

		// Voice is now playing
		voice.m_state = Voice::kPlaying;
		++m_voiceCount;

		// Store (new) index in key slot
		SFM_ASSERT(-1 == GetVoice(key));
		SetKey(key, 0);
	}

	/* ----------------------------------------------------------------------------------------------------

		Voice logic handling, (to be) called by Render().

	 ------------------------------------------------------------------------------------------------------ */

	// Prepare voices for Render() pass
	void Bison::UpdateVoicesPreRender()
	{
		m_modeSwitch = m_curVoiceMode != m_patch.voiceMode;
		const bool monophonic = Patch::VoiceMode::kMono == m_curVoiceMode;

		/*
			If voice mode changes, first steal all active voices and let a Render() pass run
		*/

		if (true == m_modeSwitch || true == m_resetVoices)
		{
			if (true == m_modeSwitch)
				Log("Voice mode switch (stealing voices)");

			if (true == m_resetVoices)
				Log("Asked to reset all voices");

			// Steal *all* active voices
			for (unsigned iVoice = 0; iVoice < kMaxVoices; ++iVoice)
			{
				Voice &voice = m_voices[iVoice];
				
				if (false == voice.IsIdle() && false == voice.IsStolen())
				{
					StealVoice(iVoice);

					Log("Voice mode switch / Voice reset, stealing voice: " + std::to_string(iVoice));
				}
			}

			m_voiceReq.clear();
			m_voiceReleaseReq.clear();

			// Set voice mode state
			m_curVoiceMode = m_patch.voiceMode;

			// Monophonic?
			if (Patch::VoiceMode::kMono == m_curVoiceMode)
			{
				// Set polyphony
				m_curPolyphony = 1;

				// Clear sequence
				m_monoReq.clear();
			}
			
			// Release stolen voices
			return;
		}

		/*
			Handle all release requests (polyphonic)
		*/

		if (false == monophonic)
		{
			std::deque<VoiceReleaseRequest> remainder;
		
			for (auto key : m_voiceReleaseReq)
			{
				const int index = GetVoice(key);
			
				// Voice still allocated?
				if (index >= 0)
				{
					Voice &voice = m_voices[index];

					if (false == voice.IsSustained())
					{
						// Voice *not* releasing?
						if (true != voice.IsReleasing())
						{
							// Release voice
							ReleaseVoice(index);
						}
					}
					else
					{
						// Voice is sustained, defer request
						remainder.emplace_back(key);
					}
				}
			}

			m_voiceReleaseReq = remainder;
		}
		
		/*
			Update real-time voice parameters
		*/

		for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
		{
			Voice &voice = m_voices[iVoice];
			{
				// Active?
				if (false == voice.IsIdle())
				{
					// Playing and not stolen?
					if (false == voice.IsDone() && false == voice.IsStolen())
					{
						// Still bound to a key?
						// FIXME: I could do this for all voices that satisfy the condition above (and just retain the last known amplitude, issue created 16/09/2020)
						if (-1 != voice.m_key)
						{
							// Update active voice:
							// - Each (active) operator has a set of parameters that need per-sample interpolation 
							// - Most of these are updated in this loop
							// - The set of parameters (also outside of this object) isn't conclusive and may vary depending on the use of FM. BISON (currently: VST plug-in)

							for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
							{
								auto &voiceOp = voice.m_operators[iOp];

								// Update per-sample interpolated parameters
								if (true == voiceOp.enabled)
								{
									const float fundamentalFreq = voice.m_fundamentalFreq;
									const PatchOperators::Operator &patchOp = m_patch.operators.operators[iOp];

									// Get velocity & frequency
									const float opVelocity = (false == patchOp.velocityInvert) ? voice.m_velocity : 1.f-voice.m_velocity;
									const float frequency = CalcOpFreq(fundamentalFreq, voiceOp.detuneOffs, patchOp);

									// Get amplitude & index
									const float level = CalcOpLevel(voice.m_key, opVelocity, patchOp);
									const float amplitude = patchOp.output*level, index = patchOp.output*level; /* FIXME: patchOp.index*level; */
								
									// Interpolate freq. if necessary
									if (frequency != voiceOp.setFrequency)
									{
										voiceOp.curFreq.SetTarget(frequency);
										voiceOp.setFrequency = frequency;
									}

									// Set amplitude & index
									voiceOp.amplitude.SetTarget(amplitude);
									voiceOp.index.SetTarget(index);

									// Square(pusher) (or "drive")
									const float drive = lerpf<float>(patchOp.drive, patchOp.drive*opVelocity, patchOp.velSens);
									voiceOp.drive.SetTarget(drive);

									// Feedback amount
									voiceOp.feedbackAmt.SetTarget(patchOp.feedbackAmt);
					
									// Panning (as set by static parameter)
									voiceOp.panning.SetTarget(CalcPanning(patchOp));
								}
							}
						}
						else
						{
							// Not bound to key: voice must be releasing (though after a recent (13/05/2020) adjustment this should no longer happen in polyphonic mode)
							SFM_ASSERT(true == voice.IsReleasing());
						}
					}
				}
			}
		}
		
		if (false == monophonic)
		{
			/* Polyphonic */

			// Sort list by time stamp
			// The front of the deque will be the first (smallest) time stamp; we'll honour requests in that order
			std::sort(m_voiceReq.begin(), m_voiceReq.end(), [](const auto &left, const auto &right) -> bool { return left.timeStamp < right.timeStamp; } );
			
			// Allocate voices (simple first-fit)
			while (m_voiceReq.size() > 0 && m_voiceCount < m_curPolyphony)
			{
				// Pick first free voice
				for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
				{
					Voice &voice = m_voices[iVoice];

					if (true == voice.IsIdle())
					{
						// Initialize (also pops request)
						InitializeVoice(iVoice);
						break;
					}
				}
			}
			
			// If we still have requests, try to steal (releasing or sustaining) voices in order to 
			// free up slots that can be used to spawn these voices the next frame (no gaurantee though!)

			size_t remainingRequests = m_voiceReq.size();

			if (remainingRequests > 0)
			{
				struct VoiceRef
				{
					unsigned iVoice;
					float summedOutput;
				};

				std::vector<VoiceRef> voiceRefs;

				for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
				{
					Voice &voice = m_voices[iVoice];

					if (false == voice.IsStolen())
					{
						const bool isReleasing = voice.IsReleasing();
						const bool isSustained = voice.IsSustained();
						
						// Only consider releasing or sustained voices
						if (true == isReleasing || true == isSustained)
						{
							VoiceRef voiceRef;
							voiceRef.iVoice = iVoice;
							voiceRef.summedOutput = voice.GetSummedOutput();

							voiceRefs.emplace_back(voiceRef);
						}
					}
				}

				// Sort list from low to high summed output
				std::sort(voiceRefs.begin(), voiceRefs.end(), [](const auto &left, const auto &right ) -> bool { return left.summedOutput < right.summedOutput; } );

				for (auto &voiceRef : voiceRefs)
				{
					// Steal voice
					const unsigned iVoice = voiceRef.iVoice;
					StealVoice(iVoice);
					Log("Voice stolen (index): " + std::to_string(iVoice));
					
					if (--remainingRequests == 0)
						break;
				}

				if (remainingRequests != 0)
				{
					// FIXME: I think it's a viable strategy to drop the remaining requests?
					Log("Could not steal enough voices: " + std::to_string(remainingRequests) + " remaining.");
				}
			}

			// Now first in line to be allocated next frame
			for (auto &request : m_voiceReq)
				request.timeStamp = 0;
		}
		else
		{
			/* Monophonic */

			const auto &voice = m_voices[0];

			// Voice request?
			if (false == m_voiceReq.empty())
			{
				// One at a time
				SFM_ASSERT(1 == m_voiceReq.size());
				
				// Current key
				const auto key = voice.m_key;

				// Copy and quickly fade if it's releasing
				if (false == voice.IsIdle() && true == voice.IsReleasing())
				{
					m_voices[1] = m_voices[0]; // Copy
					m_voices[1].m_key = -1;    // Unbind
					StealVoice(1);             // Steal
				}

				// Initialize new voice immediately
				if (-1 != key)
					FreeKey(key);
				
				if (m_voiceCount > 1)
					--m_voiceCount;

				InitializeVoice(0);

				// New voice: clear release request
				m_voiceReleaseReq.clear();
			}
			else if (false == m_voiceReleaseReq.empty()) // No voice request, is there a release request?
			{
				// One at a time
				SFM_ASSERT(m_voiceReleaseReq.size() <= 1);

				const auto key = m_voiceReleaseReq[0];
				const int index = GetVoice(key);
			
				// Voice allocated? (FIXME: it should be, assertion?)
				if (index >= 0)
				{
					Voice &voiceToRelease = m_voices[index];
					
					// Voice not sustained?
					if (false == voiceToRelease.IsSustained())
					{
						// Voice not releasing?
						if (true != voiceToRelease.IsReleasing())
						{
							ReleaseVoice(index);
						}

						// Request honoured
						m_voiceReleaseReq.clear();
					}
				}
			}

			// Rationale: second voice may only be used to quickly cut the previous voice
			SFM_ASSERT(true == m_voices[1].IsIdle() || true == m_voices[1].IsStolen());
		}
	}

	// Update voices after Render() pass
	void Bison::UpdateVoicesPostRender()
	{
		// Free (stolen) voices
		for (unsigned iVoice = 0; iVoice < kMaxVoices /* Evaluate all! */; ++iVoice)
		{
			Voice &voice = m_voices[iVoice];

			if (false == voice.IsIdle())
			{
				const bool isDone   = voice.IsDone(); 
				const bool isStolen = voice.IsStolen();

				if (true == isDone || true == isStolen)
				{
					FreeVoice(iVoice);

					// Full reset after switch
					if (true == m_modeSwitch)
					{
						voice.Reset(m_sampleRate);
					}
				}
			}
		}

		// Possible mode switch complete
		m_modeSwitch = false;
		
		// Voice request should be honoured immediately in monophonic mode
		const bool monophonic = Patch::VoiceMode::kMono == m_curVoiceMode;
		SFM_ASSERT(false == monophonic || true == m_voiceReq.empty());
	}

	// Update sustain state
	void Bison::UpdateSustain()
	{
		if (Patch::kNoPedal == m_patch.sustainType || Patch::kWahPedal == m_patch.sustainType)
			return;

		// Sustain of pitch envelope is taken care of in synth-voice.cpp!

		const bool state = m_sustain;

		const bool monophonic = Patch::VoiceMode::kMono == m_curVoiceMode;

		if (Patch::kSynthPedal == m_patch.sustainType || true == monophonic /* Monophonic *always* uses synthesizer style pedal mode */)
		{
			/*
				Emulate the synthesizer type behaviour (like the DX7) which means that the envelope runs until it hits the sustain phase
				and remains there until the pedal is lifted before it releases the voices; pressure is ignored.
			*/

			if (true == state)
			{
				// Sustain all playing voices (ignore NOTE_OFF)
				for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
				{
					Voice &voice = m_voices[iVoice];
					if (true == voice.IsPlaying() && false == voice.IsSustained())
					{
						voice.m_sustained = true;
						Log("Voice sustained (synth.): " + std::to_string(iVoice));
					}
				}
			}
			else
			{
				// Release all sustained (playing) voices
				for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
				{
					Voice &voice = m_voices[iVoice];
					if (true == voice.IsPlaying() && true == voice.IsSustained())
					{
						voice.m_sustained = false;
						Log("Voice no longer sustained (synth.): " + std::to_string(iVoice));
					}
				}
			}
		}
		else if (Patch::kPianoPedal == m_patch.sustainType)
		{
			/*
				Emulation of piano (or CP) behaviour.
			*/

			const float pedalFalloff    = m_patch.pianoPedalFalloff;
			const float pedalReleaseMul = m_patch.pianoPedalReleaseMul;
			
			if (true == state)
			{
				// Sustain all playing voices
				for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
				{
					Voice &voice = m_voices[iVoice];
					if (true == voice.IsPlaying() && false == voice.IsSustained())
					{
						voice.m_sustained = true;

						// Pitch envelope is taken care of in Voice::Sample()
						voice.m_filterEnvelope.OnPianoSustain(m_sampleRate, pedalFalloff, pedalReleaseMul);

						for (auto& voiceOp : voice.m_operators)
							if (true == voiceOp.enabled && true == voiceOp.isCarrier)
							{
								voiceOp.envelope.OnPianoSustain(m_sampleRate, pedalFalloff, pedalReleaseMul);
							}

						Log("Voice sustained (CP): " + std::to_string(iVoice));
					}
				}
			}
			else
			{
				// Release all sustained (playing) voices
				for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
				{
					Voice &voice = m_voices[iVoice];
					if (false == voice.IsIdle() && true == voice.IsSustained())
					{
						voice.m_sustained = false;
						Log("Voice no longer sustained (CP): " + std::to_string(iVoice));
					}
				}
			}
		}
	}

	/* ----------------------------------------------------------------------------------------------------

		Voice render.

		This function was lifted straight out of Render() so it has a few dependencies on class members
		instead of just the context(s). I've made the function constant as a precaution but it's not
		unthinkable that something slips through the cracks. 
		
		If you spot anything fishy please fix it ASAP.

	 ------------------------------------------------------------------------------------------------------ */

	/* static */ void Bison::VoiceRenderThread(Bison *pInst, VoiceThreadContext *pContext)
	{
		SFM_ASSERT(nullptr != pInst);
		SFM_ASSERT(nullptr != pContext);
		pInst->RenderVoices(pContext->parameters, pContext->voiceIndices, pContext->numSamples, pContext->pDestL, pContext->pDestR);
	}

	// Renders a set of voices
	// - Stick to variables supplied through a context *or* make very sure you read only!
	// - Assumes that each voice is active
	void Bison::RenderVoices(const VoiceRenderParameters &context, const std::vector<unsigned> &voiceIndices, unsigned numSamples, float *pDestL, float *pDestR) const
	{
		SFM_ASSERT(nullptr != pDestL && nullptr != pDestR);

		for (auto iVoice : voiceIndices)
		{
			Voice &voice = const_cast<Voice&>(m_voices[iVoice]);
			SFM_ASSERT(false == voice.IsIdle());

			// Global per-voice gain
			constexpr float voiceGain = 0.354813397f; // dBToGain(kVoiceGaindB);

			// Update LFO frequencies
			float frequency = m_globalLFO->GetFrequency(), modFrequency;
			CalcLFOFreq(frequency, modFrequency, m_patch.LFOModSpeed);
			
			voice.m_LFO1.SetFrequency(frequency);
			voice.m_LFO2.SetFrequency(frequency);
			voice.m_modLFO.SetFrequency(modFrequency);
			
			// Update LFO S&H parameters
			const float slewRate = m_SandHSlewRatePS.Get();
			voice.m_LFO1.SetSampleAndHoldSlewRate(slewRate);
			voice.m_LFO2.SetSampleAndHoldSlewRate(slewRate);
			voice.m_modLFO.SetSampleAndHoldSlewRate(slewRate);

			// Global amp. allows use to fade the voice in and out within this frame
			InterpolatedParameter<kLinInterpolate> globalAmp(1.f, numSamples);

			if (true == voice.IsStolen())
			{
				// If stolen, fade out completely in this Render() pass
				globalAmp.SetTarget(0.f);
			}
			else
			{
				if (true == m_resetPhaseBPM)
				{
					// If resetting BPM sync. phase, fade in in this Render() pass
					globalAmp.Set(0.f);
					globalAmp.SetTarget(1.f);
				}
			}
	
			if (true == context.resetFilter)
			{
				// Reset
				voice.m_filterSVF1.resetState();
				voice.m_filterSVF2.resetState();
			}

			// LFO
			auto curLFOBlend    = m_curLFOBlend;
			auto curLFOModDepth = m_curLFOModDepth;

			// Reset to initial cutoff & Q
			auto curCutoff = m_curCutoff;
			auto curQ      = m_curQ;

			// Reset to initial pitch/amp. bend & modulation
			auto curPitchBend  = m_curPitchBend;
			auto curAmpBend    = m_curAmpBend;
			auto curModulation = m_curModulation;

			// Reset to initial aftertouch
			auto curAftertouch = m_curAftertouch;

			// Reset to initial global amp.
			auto curGlobalAmp = globalAmp;

			const bool noFilter = SvfLinearTrapOptimised2::NO_FLT_TYPE == context.filterType1;
			auto& filterEG      = voice.m_filterEnvelope;

			// Second filter lags behind a little to add a bit of "gritty sparkle"
			SinglePoleLPF secondCutoffLPF(SVF_CutoffToHz(0.9f, m_Nyquist)/m_sampleRate);
			secondCutoffLPF.Reset(curCutoff.Get());
					
			for (unsigned iSample = 0; iSample < numSamples; ++iSample)
			{
				const float sampAftertouch = curAftertouch.Sample();

				const float sampMod = std::min<float>(1.f, curModulation.Sample() + context.modulationAftertouch*sampAftertouch);
						
				// Render dry voice
				float left, right;
				voice.Sample(
					left, right, 
					curPitchBend.Sample(),
					curAmpBend.Sample(),
					sampMod,
					curLFOBlend.Sample(), 
					curLFOModDepth.Sample());

				// Sample filter envelope
				float filterEnv = filterEG.Sample();
				if (true == m_patch.filterEnvInvert)
					filterEnv = 1.f-filterEnv;

				// SVF cutoff aftertouch (curved towards zero if pressed)
				const float cutAfter = context.mainFilterAftertouch*sampAftertouch;
				SFM_ASSERT(cutAfter >= 0.f && cutAfter <= 1.f);

#if !defined(SFM_DISABLE_FX)						

				// Apply & mix filter (FIXME: move to sequential loop?)
				if (false == noFilter)
				{	
					float filteredL = left;
					float filteredR = right;
							
					// Cutoff & Q, finally, for *this* sample
					const float nonEnvCutoffHz = curCutoff.Sample()*(1.f - cutAfter*kMainCutoffAftertouchRange); // More pressure -> lower cutoff freq.
					const float cutoffHz = lerpf<float>(context.fullCutoff, nonEnvCutoffHz, filterEnv);
					const float sampQ = curQ.Sample();

					if (true == context.secondFilterPass)
					{
						// Currently we're only using this for the LPF, which allows for a minor optimization
						SFM_ASSERT(SvfLinearTrapOptimised2::LOW_PASS_FILTER == context.filterType2);

						const float secondQ = std::min<float>(kSVFMaxFilterQ, sampQ+context.secondQOffs);
						voice.m_filterSVF2.updateLowpassCoeff(secondCutoffLPF.Apply(cutoffHz), secondQ, m_sampleRate);
//						voice.m_filterSVF2.updateCoefficients(secondCutoffLPF.Apply(cutoffHz), secondQ, context.filterType2, m_sampleRate);
						voice.m_filterSVF2.tick(filteredL, filteredR);
					}

					// Ref.: https://github.com/FredAntonCorvest/Common-DSP/blob/master/Filter/SvfLinearTrapOptimised2Demo.cpp
					voice.m_filterSVF1.updateCoefficients(cutoffHz, sampQ, context.filterType1, m_sampleRate);
					voice.m_filterSVF1.tick(filteredL, filteredR);
							
					left  = filteredL;
					right = filteredR;
				}

#endif

				// Apply gain and add to mix
				const float amplitude = curGlobalAmp.Sample() * voiceGain;
				pDestL[iSample] += amplitude * left;
				pDestR[iSample] += amplitude * right;
			}
		}
	}

	/* ----------------------------------------------------------------------------------------------------

		Block renderer; basically takes care of all there is to it in the right order.
		Currently tailored to play nice with (JUCE) VST.

	 ------------------------------------------------------------------------------------------------------ */
	
	void Bison::Render(unsigned numSamples, float bendWheel, float modulation, float aftertouch, float *pLeft, float *pRight)
	{
		SFM_ASSERT(bendWheel  >= -1.f && bendWheel  <= 1.f);
		SFM_ASSERT(modulation >=  0.f && modulation <= 1.f);
		SFM_ASSERT(aftertouch >=  0.f && aftertouch <= 1.f);

		SFM_ASSERT(nullptr != pLeft && nullptr != pRight);
		SFM_ASSERT(nullptr != m_pBufL[0] && nullptr != m_pBufR[0]);
		SFM_ASSERT(nullptr != m_pBufL[1] && nullptr != m_pBufR[1]);

		if (numSamples > m_samplesPerBlock)
		{
			// According to JUCE's documentation this *can* occur, but I don't think it ever does (nor should)
			SFM_ASSERT(false);
			return;
		}

#if SFM_KILL_DENORMALS
		// Disable denormals
		DisableDenormals disableDEN;
#endif

		const bool monophonic = Patch::VoiceMode::kMono == m_curVoiceMode;

		// Reset voices if polyphony changes (monophonic switch is handled in UpdateVoicesPreRender())
		if (false == monophonic)
		{
			if (m_curPolyphony != m_patch.maxVoices)
			{
				m_resetVoices = true;
				m_curPolyphony = m_patch.maxVoices;
			}
		}

		// Modulation override?
		if (0.f != m_patch.modulationOverride)
		{
			SFM_ASSERT(m_patch.modulationOverride > 0.f && m_patch.modulationOverride <= 1.f);
			modulation = m_patch.modulationOverride;
		}

		// Calculate current BPM freq.
		unsigned overrideDelayBit = 0;
		if (true == m_patch.beatSync && 0.f != m_BPM)
		{
			const float ratio = m_patch.beatSyncRatio; // Note ratio
			SFM_ASSERT(ratio >= 0);

			const float BPM = m_BPM;
			const float BPS = BPM/60.f;    // Beats per sec.
			m_freqBPM = BPS/ratio;         // Sync. freq.
			
			// If can't fit delay within it's line, revert to manual setting
			if (1.f/m_freqBPM >= kMainDelayInSec)
				overrideDelayBit = kFlagOverrideDelay;
		}
		else
			// None: interpret this as a cue to use user controlled rate(s)
			m_freqBPM = 0.f;

		// Calculate LFO freq.
		float freqLFO = 0.f;

		const bool overrideLFO = m_patch.syncOverride & kFlagOverrideLFO;
		if (false == m_patch.beatSync || m_freqBPM == 0.f || true == overrideLFO)
		{
			// Set LFO speed in (DX7) range
			freqLFO = MIDI_To_DX7_LFO_Hz(m_patch.LFORate);
			m_globalLFO->SetFrequency(m_LFORatePS.Apply(freqLFO));
		}
		else
		{
			// Adapt BPM freq.
			freqLFO = m_freqBPM;

			if (false == m_resetPhaseBPM)
			{
				m_globalLFO->SetFrequency(m_LFORatePS.Apply(freqLFO));
			}
			else
			{
				// Full reset; likely to be used when (re)starting a track
				// This *must* be done prior to UpdateVoicesPreRender()
				m_globalLFO->Initialize(freqLFO, m_sampleRate);

				// (Re)set ParameterSlew
				m_LFORatePS.Reset(freqLFO); 
			}
		}
		
		// Filter/Prepare LFO & S&H parameters (so they can be used by RenderVoices())
		m_curLFOBlend.SetTarget(m_LFOBlendPS.Apply(m_patch.LFOBlend));
		m_curLFOModDepth.SetTarget(m_LFOModDepthPS.Apply(m_patch.LFOModDepth));
		m_SandHSlewRatePS.Apply(m_patch.SandHSlewRate); // Does not need per-sample interpolation

		// Update voice logic (pre)
		UpdateVoicesPreRender();

		// Update filter type & state
		// This is where the magic happens ;)

		SvfLinearTrapOptimised2::FLT_TYPE filterType1; // Actually the *second* step if 'secondFilterpass' is true, and the only if it's false
		SvfLinearTrapOptimised2::FLT_TYPE filterType2 = SvfLinearTrapOptimised2::NO_FLT_TYPE;
		
		// Set target cutoff (Hz) & Q
		const float normCutoff = m_cutoffPS.Apply(m_patch.cutoff);
		const float resonance = m_resoPS.Apply(m_patch.resonance);

		// Using smoothstepf() to add a little curvature, chiefly intended to appease basic MIDI controls
		const float cutoff = SVF_CutoffToHz(smoothstepf(normCutoff), m_Nyquist);
		m_curCutoff.SetTarget(cutoff);
		
		// Set filter type & parameters
		float normQ = resonance;

		bool secondFilterPass  = false;                          // Second pass
		float secondQOffs      = 0.f;                            // Q offset for second stage
		const float fullCutoff = SVF_CutoffToHz(1.f, m_Nyquist); // Full cutoff used to apply DCF

		float Q;
		switch (m_patch.filterType)
		{
		default:
		case Patch::kNoFilter:
			filterType1 = SvfLinearTrapOptimised2::NO_FLT_TYPE;
			Q = SVF_ResoToQ(normQ*m_patch.resonanceLimit);
			break;

		case Patch::kLowpassFilter:
			// Screams and yells
			secondFilterPass = true;
			secondQOffs = 0.1f;
			filterType1 = SvfLinearTrapOptimised2::LOW_PASS_FILTER;
			filterType2 = SvfLinearTrapOptimised2::LOW_PASS_FILTER;
			Q = SVF_ResoToQ(normQ*m_patch.resonanceLimit);
			break;

		case Patch::kHighpassFilter:
			filterType1 = SvfLinearTrapOptimised2::HIGH_PASS_FILTER;
			Q = SVF_ResoToQ(normQ*m_patch.resonanceLimit);
			break;

		case Patch::kBandpassFilter:
			filterType1 = SvfLinearTrapOptimised2::BAND_PASS_FILTER;
			Q = SVF_ResoToQ(0.25f*normQ); // Lifts
			break;

		case Patch::kNotchFilter:
			filterType1 = SvfLinearTrapOptimised2::NOTCH_FILTER;
			Q = SVF_ResoToQ(0.25f - 0.25f*normQ); // Dents
			break;
		}
		
		// Set correct Q
		m_curQ.SetTarget(Q);

		// Switched filter type?
		const bool resetFilter = m_curFilterType != filterType1;
		m_curFilterType = filterType1;

		// Set pitch & amp. wheel & modulation target values
		const float bendWheelFiltered = m_bendWheelPS.Apply(bendWheel);
		if (false == m_patch.pitchIsAmpMod)
		{
			// Wheel modulates pitch
			m_curPitchBend.SetTarget(bendWheelFiltered);
			m_curAmpBend.SetTarget(1.f /* 0dB */);
		}
		else
		{
			// Wheel modulates amplitude
			m_curAmpBend.SetTarget(dB2Lin(bendWheelFiltered*kAmpBendRange));
			m_curPitchBend.SetTarget(0.f);
		}

		// Set modulation & aftertouch target values
		m_curModulation.SetTarget(m_modulationPS.Apply(modulation));

		const float aftertouchFiltered = m_aftertouchPS.Apply(aftertouch);
		m_curAftertouch.SetTarget(aftertouchFiltered);

		// Clear L/R buffers
		memset(m_pBufL[0], 0, m_samplesPerBlock*sizeof(float));
		memset(m_pBufR[0], 0, m_samplesPerBlock*sizeof(float));

		// Start rendering voices, if necessary
		const unsigned numVoices = m_voiceCount;

		if (0 != numVoices)
		{
			// Swap 2 branches for multiplications later on
			const float mainFilterAftertouch = (Patch::kMainFilter == m_patch.aftertouchMod) ? 1.f : 0.f;
			const float modulationAftertouch = (Patch::kModulation == m_patch.aftertouchMod) ? 1.f : 0.f;

			VoiceRenderParameters parameters;
			parameters.freqLFO = freqLFO;
			parameters.filterType1 = filterType1;
			parameters.filterType2 = filterType2;
			parameters.resetFilter = resetFilter;
			parameters.secondFilterPass = secondFilterPass;
			parameters.secondQOffs = secondQOffs;
			parameters.fullCutoff = fullCutoff;
			parameters.modulationAftertouch = modulationAftertouch;
			parameters.mainFilterAftertouch = mainFilterAftertouch;

			// Build array of voices to render
			std::vector<unsigned> voiceIndices;
			for (int iVoice = 0; iVoice < kMaxVoices /* Actual voice count can be > m_curPolyphony */; ++iVoice)
			{
				if (false == m_voices[iVoice].IsIdle())
					voiceIndices.push_back(iVoice);
			}

#if defined(SFM_DISABLE_VOICE_THREAD)
			if (true)
#else
			if (voiceIndices.size() <= kSingleThreadMaxVoices)
#endif
			{
				// Render all voices on current thread
				VoiceThreadContext context(parameters);
				context.voiceIndices = std::move(voiceIndices);
				context.numSamples = numSamples;
				context.pDestL = m_pBufL[0];
				context.pDestR = m_pBufR[0];
			
				VoiceRenderThread(this, &context);
			}
			else
			{
				// Clear L/R buffers
				memset(m_pBufL[1], 0, m_samplesPerBlock*sizeof(float));
				memset(m_pBufR[1], 0, m_samplesPerBlock*sizeof(float));
				
				// Use 2 (current plus one extra) threads to render voices
				VoiceThreadContext contexts[2] = { parameters, parameters };
				
				// Split voices up 50/50
				const size_t half = voiceIndices.size();
				contexts[0].voiceIndices = std::vector<unsigned>(voiceIndices.begin(), voiceIndices.begin() + half);
				contexts[1].voiceIndices = std::vector<unsigned>(voiceIndices.begin() + half, voiceIndices.end());

				contexts[0].numSamples = contexts[1].numSamples = numSamples;

				contexts[0].pDestL = m_pBufL[0];
				contexts[0].pDestR = m_pBufR[0];
				contexts[1].pDestL = m_pBufL[1];
				contexts[1].pDestR = m_pBufR[1];

				std::thread thread(VoiceRenderThread, this, &contexts[1]);

				VoiceRenderThread(this, &contexts[0]); // Process our part
				thread.join();                         // Wait for thread

				// Mix samples (FIXME: could move to PostPass but if all is well we've already won at least *some* CPU if necessary)
				for (unsigned iSample = 0; iSample < numSamples; ++iSample)
				{
					m_pBufL[0][iSample] += m_pBufL[1][iSample];
					m_pBufR[0][iSample] += m_pBufR[1][iSample];
				}
			}
		}
		
		// Keep *all* supersaw oscillators running; I could move this loop to RenderVoices(), but that would clutter up the function a bit,
		// and here it's easy to follow and easy to extend
		for (auto &voice : m_voices)
		{	
			const bool isIdle = voice.IsIdle();

			for (auto &voiceOp : voice.m_operators)
			{
				// Only update if *not* in use
				if (true == isIdle || false == voiceOp.enabled)
				{
					auto &saw = voiceOp.oscillator.GetSupersaw();
					saw.Skip(numSamples);
				}
			}
		}

		// Advance global LFO phase (free running)
		m_globalLFO->Skip(numSamples);
				
		// Update voice logic (post)
		UpdateVoicesPostRender();

		// Update sustain state
		UpdateSustain();

		// Handle auto-wah ("sustain") pedal case
		if (Patch::kWahPedal == m_patch.sustainType)
		{
			// Apply slew
			m_autoWahPedalPS.Apply( (true == m_sustain) ? 1.f : 0.f );
		}
		else
			// Full effect
			m_autoWahPedalPS.Apply(1.f);

		// Calc. post filter wetness
		float postWet = m_patch.postWet;
		if (Patch::kPostFilter == m_patch.aftertouchMod)
			postWet = std::min<float>(1.f, postWet+aftertouchFiltered); // More pressure -> more wetness

		// Apply post-processing (FIXME: pass structure?)
		m_postPass->Apply(numSamples,
			/* BPM sync. */
			m_freqBPM, m_patch.syncOverride | overrideDelayBit,
			/* Auto-wah (FIXME: use more ParameterSlew if necessary) */
			m_patch.wahResonance,
			m_patch.wahAttack,
			m_patch.wahHold,
			m_wahRatePS.Apply(m_patch.wahRate),
			m_wahDrivePS.Apply(m_patch.wahDrivedB),
			m_wahSpeakPS.Apply(m_patch.wahSpeak),
			m_wahSpeakVowelPS.Apply(m_patch.wahSpeakVowel),
			m_wahSpeakVowelModPS.Apply(m_patch.wahSpeakVowelMod),
			m_wahSpeakGhostPS.Apply(m_patch.wahSpeakGhost),
			m_wahSpeakCutPS.Apply(m_patch.wahSpeakCut),
			m_wahSpeakResoPS.Apply(m_patch.wahSpeakResonance),
			m_wahCutPS.Apply(m_patch.wahCut),
			m_wahWetPS.Apply(m_patch.wahWet*m_autoWahPedalPS.Get()),
			/* Chorus/Phaser */
			m_effectRatePS.Apply(m_patch.cpRate),
			m_effectWetPS.Apply(m_patch.cpWet),
			false == m_patch.cpIsPhaser,
			/* Delay */
			m_delayPS.Apply(m_patch.delayInSec),
			m_delayWetPS.Apply(m_patch.delayWet),
			m_delayDrivePS.Apply(m_patch.delayDrivedB),
			m_delayFeedbackPS.Apply(m_patch.delayFeedback),
			m_delayFeedbackCutoffPS.Apply(m_patch.delayFeedbackCutoff),
			m_delayTapeWowPS.Apply(m_patch.delayTapeWow),
			/* MOOG-style 24dB filter + Tube distort */
			m_postCutoffPS.Apply(m_patch.postCutoff),
			m_postResoPS.Apply(m_patch.postResonance),
			m_postDrivePS.Apply(m_patch.postDrivedB),
			m_postWetPS.Apply(postWet),
			m_tubeDistPS.Apply(m_patch.tubeDistort),
			m_tubeDrivePS.Apply(m_patch.tubeDrive),
			m_patch.tubeOffset, // Does not crackle without ParameterSlew
			/* Reverb */
			m_reverbWetPS.Apply(m_patch.reverbWet),
			m_reverbRoomSizePS.Apply(m_patch.reverbRoomSize),
			m_reverbDampeningPS.Apply(m_patch.reverbDampening),
			m_reverbWidthPS.Apply(m_patch.reverbWidth),
			m_reverbBassTuningdBPS.Apply(m_patch.reverbBassTuningdB),
			m_reverbTrebleTuningdBPS.Apply(m_patch.reverbTrebleTuningdB),
			m_reverbPreDelayPS.Apply(m_patch.reverbPreDelay),
			/* Compressor (FIXME: use more ParameterSlew if necessary) */
			m_patch.compThresholddB,
			m_patch.compKneedB,
			m_patch.compRatio,
			m_patch.compGaindB,
			m_patch.compAttack,
			m_patch.compRelease,
			m_compLookaheadPS.Apply(m_patch.compLookahead),
			m_patch.compAutoGain,
			m_patch.compRMSToPeak,
			/* Tuning (post-EQ) */
			m_bassTuningdBPS.Apply(m_patch.bassTuningdB),
			m_trebleTuningdBPS.Apply(m_patch.trebleTuningdB),
			m_midTuningdBPS.Apply(m_patch.midTuningdB),
			/* Master volume */
			m_masterVoldBPS.Apply(m_patch.masterVoldB),
			/* Buffers */
			m_pBufL[0], m_pBufR[0], pLeft, pRight);

		// Of all these, copies were used per voice, so skip numSamples to keep up	
		m_curLFOBlend.Skip(numSamples);
		m_curLFOModDepth.Skip(numSamples);
		m_curCutoff.Skip(numSamples);
		m_curQ.Skip(numSamples);
		m_curPitchBend.Skip(numSamples);
		m_curAmpBend.Skip(numSamples);
		m_curModulation.Skip(numSamples);
		m_curAftertouch.Skip(numSamples);

		// This has been done by now
		m_resetVoices   = false;
		m_resetPhaseBPM = false;

		// Calculate peak ([0..1]) for each operator (for visualization purposes, plus it's not very pretty, FIXME)
		if (numVoices > 0)
		{
			// Find max. gain for each operator
			float maxGains[kNumOperators] = { 0.f };

			for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
			{
				Voice &voice = m_voices[iVoice];

				if (false == voice.IsIdle())
				{
					for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
					{
						Voice::Operator &voiceOp = voice.m_operators[iOp];

						if (true == voiceOp.enabled)
						{
							const float curGain = voiceOp.envGain.Get();
							
							// New maximum?
							if (curGain > maxGains[iOp])
								maxGains[iOp] = curGain;
						}
					}
				}
			}

			// Apply max. gains
			for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
				m_opPeaksEnv[iOp].Apply(maxGains[iOp], m_opPeaks[iOp]);
		}
		else if (0 == numVoices)
		{
			// Decay towards zero
			for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
				m_opPeaksEnv[iOp].Apply(0.f, m_opPeaks[iOp]);
		}
	}

}; // namespace SFM
