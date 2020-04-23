
/*
	FM. BISON hybrid FM synthesis
	(C) visualizers.nl & bipolar audio
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

// Shut it, MSVC
#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

// C++11
#include <mutex>
#include <shared_mutex>
// #include <deque>
// #include <cfenv>

#include "FM_BISON.h"

#include "synth-MIDI.h"
#include "synth-patch-global.h"
#include "synth-math.h"
// #include "synth-voice.h"
// #include "synth-one-pole-filters.h"
// #include "synth-post-pass.h"
// #include "synth-phase.h"
#include "synth-distort.h"
// #include "synth-interpolated-parameter.h"
#include "synth-DX7-LFO-table.h"

namespace SFM
{
	/* ----------------------------------------------------------------------------------------------------

		Constructor/Destructor

	 ------------------------------------------------------------------------------------------------------ */
	
	Bison::Bison()
	{
		static bool s_staticInit = true;
		if (true == s_staticInit)
		{
			// Calculate LUTs & initialize random generator
			InitializeRandomGenerator();
			CalculateMIDIToFrequencyLUT();
			InitializeFastCosine();
			Oscillator::CalculateSupersawDetuneTable();

			s_staticInit = false;
		}
		
		// Reset entire patch
		m_patch.ResetToEngineDefaults();

		// Initialize polyphony
		m_curPolyphony = m_patch.maxVoices;

		Log("FM. BISON engine initalized");
		Log("Suzie, call Dr. Bison, tell him it's for me...");

		/*
			IMPORTANT: at this point it is still necessary to call SetSamplingProperties(), which the VST plug-in will take
					   care of for now.
		*/	
	}

	Bison::~Bison() 
	{
		DeleteRateDependentObjects();

		Log("FM. BISON engine released");
	}

	/* ----------------------------------------------------------------------------------------------------

		OnSetSamplingProperties() is called by JUCE, but what it does is set up the basic properties
		necessary to start rendering so it can be used in any situation.

	 ------------------------------------------------------------------------------------------------------ */

	// Called by JUCE's prepareToPlay()
	void Bison::OnSetSamplingProperties(unsigned sampleRate, unsigned samplesPerBlock)
	{
		m_sampleRate       = sampleRate;
		m_samplesPerBlock  = samplesPerBlock;
		
		// Sample rates higher than a certain value just give us headroom and nothing else
		m_Nyquist          = std::min<unsigned>(sampleRate>>1, unsigned(kAudibleHighHz));

		/* 
			Reset sample rate dependent global objects:
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

		// Allocate intermediate buffers
		m_pBufL = reinterpret_cast<float *>(mallocAligned(m_samplesPerBlock*sizeof(float), 16));
		m_pBufR = reinterpret_cast<float *>(mallocAligned(m_samplesPerBlock*sizeof(float), 16));

		// Create effects
		m_postPass = new PostPass(m_sampleRate, m_samplesPerBlock, m_Nyquist);
		const auto oversamplingRate = m_postPass->GetOversamplingRate();

		// Start global LFO phase
		m_globalLFO = new Phase(m_sampleRate);
		const float freqLFO = MIDI_To_LFO_Hz(m_patch.lfoRate);
		m_globalLFO->Initialize(freqLFO, m_sampleRate);

		// Reset interpolated global parameter(s)
		m_curCutoff     = { CutoffToHz(m_patch.cutoff, m_Nyquist), m_sampleRate, kDefParameterLatency };
		m_curQ          = { ResoToQ(m_patch.resonance), m_sampleRate, kDefParameterLatency };
		m_curPitchBend  = { 0.f, m_sampleRate, kDefParameterLatency };
		m_curAmpBend    = { 0.f, m_sampleRate, kDefParameterLatency };
		m_curModulation = { 0.f, m_sampleRate, kDefParameterLatency*1.5f /* Longer */ };
		m_curAftertouch = { 0.f, m_sampleRate, kDefParameterLatency*3.f /* Longer */ };

		// Set parameter filter rates to reduce noise (to default cut Hz, mostly)

		// Local
		m_cutoffPF              = { m_sampleRate };
		m_resoPF                = { m_sampleRate };

		m_cutoffPF.Reset(m_patch.cutoff);
		m_resoPF.Reset(m_patch.resonance);

		// PostPass
		m_effectWetPF           = { m_sampleRate }; 
		m_effectRatePF          = { m_sampleRate };
		m_delayPF               = { m_sampleRate, kDefParameterFilterCutHz * 0.05f /* Softens it up nicely */ };
		m_delayWetPF            = { m_sampleRate };
		m_delayFeedbackPF       = { m_sampleRate };
		m_delayFeedbackCutoffPF = { m_sampleRate };
		m_tubeDistPF            = { oversamplingRate };
		m_postCutoffPF          = { oversamplingRate };
		m_postResoPF            = { oversamplingRate };
		m_postDrivePF           = { oversamplingRate };
		m_postWetPF             = { oversamplingRate, kDefParameterFilterCutHz*0.3f /* Shorter */ };
		m_avgVelocityPF         = { oversamplingRate, kDefParameterFilterCutHz*0.1f /* Shorter */ };
		m_reverbWetPF           = { m_sampleRate };
		m_reverbRoomSizePF      = { m_sampleRate };
		m_reverbDampeningPF     = { m_sampleRate };
		m_reverbWidthPF         = { m_sampleRate };
		m_reverbHP_PF           = { m_sampleRate };
		m_reverbLP_PF           = { m_sampleRate };
		m_reverbPreDelayPF      = { m_sampleRate };
		m_compLookaheadPF       = { m_sampleRate };
		m_masterVolPF           = { m_sampleRate };

		m_effectWetPF.Reset(m_patch.cpWet);
		m_effectRatePF.Reset(m_patch.cpRate);
		m_delayPF.Reset(m_patch.delayInSec);
		m_delayWetPF.Reset(m_patch.delayWet);
		m_delayFeedbackPF.Reset(m_patch.delayFeedback);
		m_delayFeedbackCutoffPF.Reset(m_patch.delayFeedbackCutoff);
		m_postCutoffPF.Reset(m_patch.postCutoff);
		m_postResoPF.Reset(m_patch.postResonance);
		m_postDrivePF.Reset(m_patch.postDrivedB);
		m_postWetPF.Reset(m_patch.postWet);
		m_tubeDistPF.Reset(m_patch.tubeDistort);
//		m_avgVelocityPF.Reset(0.f);
		m_reverbWetPF.Reset(m_patch.reverbWet);
		m_reverbRoomSizePF.Reset(m_patch.reverbRoomSize);
		m_reverbDampeningPF.Reset(m_patch.reverbDampening);
		m_reverbWidthPF.Reset(m_patch.reverbWidth);
		m_reverbHP_PF.Reset(m_patch.reverbHP);
		m_reverbLP_PF.Reset(m_patch.reverbLP);
		m_reverbPreDelayPF.Reset(m_patch.reverbPreDelay);
//		m_compLookaheadPF.Reset(0.f);
		m_masterVolPF.Reset(m_patch.masterVol);

		// Local
		m_bendWheelPF           = { m_sampleRate, kDefParameterFilterCutHz*0.6f  };
		m_modulationPF          = { m_sampleRate, kDefParameterFilterCutHz*0.6f  };
		m_aftertouchPF          = { m_sampleRate, kDefParameterFilterCutHz*0.05f };

		m_bendWheelPF.Reset(0.f);
		m_modulationPF.Reset(0.f);
		m_aftertouchPF.Reset(0.f);
	}

	// Cleans up after OnSetSamplingProperties()
	void Bison::DeleteRateDependentObjects()
	{
		// Allocate intermediate sample buffers
		freeAligned(m_pBufL);
		freeAligned(m_pBufR);
		m_pBufL = m_pBufR = nullptr;

		// Release post-pass
		delete m_postPass;
		m_postPass = nullptr;

		// Delete global LFO
		delete m_globalLFO;
		m_globalLFO = nullptr;
	}

	/* ----------------------------------------------------------------------------------------------------

		Voice management.

		Polyphonic mode works as you'd expect.

		Monophonic mode:
		- Each note uses it's own velocity for all calculations, instead of carrying over the
		  initial one (e.g. like I can hear my TG77 do)
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
					// Release if still playing
					if (true == voice.IsPlaying())
						ReleaseVoice(index);
					
					// Disassociate voice from key
					FreeKey(voice.m_key);
					voice.m_key = -1;

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
				// Replace last request (FIXME: replace one with lowest time stamp instead?)
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
				m_voiceReq.emplace_front(request);

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

		bool releaseIssued = false;

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
			}

			// It's not relevant if this NOTE_OFF is blocked by sustain or not
			releaseIssued = true;
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

			// Released and no new request yet?
			if (true == releaseIssued && true == m_voiceReq.empty() && false == m_monoReq.empty())
			{
				/* const */ auto &voice = m_voices[0];

				float output = 0.f;
				if (false == voice.IsIdle())
					output = voice.GetSummedOutput();

				const bool isSilent = output == 0.f;

				if (false == isSilent)
				{
					// Retrigger frontmost note in sequence
					const auto &request = m_monoReq.front();
					NoteOn(request.key, request.frequency, request.velocity, timeStamp, true);
					m_monoReq.pop_front();

					Log("Mono NOTE_ON for prev. key " +  std::to_string(request.key));
				}
			}
		}
	}

	/* ----------------------------------------------------------------------------------------------------

		Voice initialization.

		There's a separate function for the monophonic voice; this means quite some code is duplicated
		in favour of keeping the core logic for both polyphonic and monophonic separated.

	 ------------------------------------------------------------------------------------------------------ */

	SFM_INLINE static float CalcKeyScaling(unsigned key, const FM_Patch::Operator &patchOp)
	{
		SFM_ASSERT(key >= 0 && key <= 127);
		const float normalizedKey = key/127.f;

		return PaulCurve(normalizedKey, patchOp.envKeyScale);

		// Linear version
//		return patchOp.envKeyScale*normalizedKey;
	}

	SFM_INLINE static float CalcCutoffScaling(unsigned key, const FM_Patch::Operator &patchOp)
	{
		SFM_ASSERT(key >= 0 && key <= 127);
		const float normalizedKey = key/127.f;

		return PaulCurve(normalizedKey, patchOp.cutoffKeyScale);

		// Linear version
//		return patchOp.cutoffKeyScale*normalizedKey;
	}

	SFM_INLINE static float CalcPanningAngle(const FM_Patch::Operator &patchOp)
	{
		SFM_ASSERT(fabsf(patchOp.panning) <= 1.f);
		const float panAngle = (patchOp.panning+1.f)*0.5f;
		return panAngle*0.25f;
	}

	// Calculate operator frequency
	float Bison::CalcOpFreq(float fundamentalFreq, const FM_Patch::Operator &patchOp)
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

			const int   coarse = patchOp.coarse; // Ratio
			const int     fine = patchOp.fine;   // Semitones
			const float detune = patchOp.detune; // Cents

			SFM_ASSERT(coarse >= kCoarseMin && coarse <= kCoarseMax);
			SFM_ASSERT(coarse != 0);
			SFM_ASSERT(abs(fine) <= kFineRange);
			SFM_ASSERT(abs(detune) <= kDetuneRange);

			if (coarse < 0)
				frequency /= abs(coarse-1);
			else if (coarse > 0)
				frequency *= coarse;
			else
				SFM_ASSERT(false); // Coarse may *never* be zero!
			
			frequency *= powf(2.f, fine/12.f);
			frequency *= powf(2.f, (detune*0.01f)/12.f);
		}

		return frequency;
	}

	// Calculate operator amplitude, also referred to in FM lingo as (modulation) "index"
	float Bison::CalcOpIndex(unsigned key, float velocity, const FM_Patch::Operator &patchOp)
	{ 
		// Calculate output in linear domain
		float output = patchOp.output;
		SFM_ASSERT(output >= 0.f && output <= 1.f);

		// Factor in velocity (exponential)
		output = lerpf<float>(output, output*velocity, patchOp.velSens);
		
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
				output = 0.f;
		}
		else if (true == patchOp.cutLeftOfLSBP && key < breakpoint)
			// Cut left
			output = 0.f;
		else if (true == patchOp.cutRightOfLSBP && key > breakpoint)
			// Cut right
			output = 0.f;
		else
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
					// Fade out by gradually interpolating towards lower output level
					output = lerpf<float>(output, output*(1.f-fabsf(amount)), factor);
				else if (amount > 0.f)
					// Fade in by adding to set output level
					output = lerpf<float>(output, std::min<float>(1.f, output+fabsf(amount)), factor);

				// Subtractive as well as additive scaling leave the output on the other side
				// of the breakpoint intact; this makes it intuitive to use this feature (I think)
			}
		}

		SFM_ASSERT(output >= 0.f && output <= 1.f);
		
		// Returning linear output, if this has to be adjusted to dB scale, this is not the place to do so
		// because it has absolutely nothing to do with our frequency modulation
		return output;
	}

	// Calculate phase jitter
	SFM_INLINE static float CalcPhaseJitter(float drift)
	{
		const float random = -0.25f + mt_randf()*0.5f; // [-90..90] deg.
		return random*drift;
	}
	
	// Initialize new voice
	void Bison::InitializeVoice(const VoiceRequest &request, unsigned iVoice)
	{
		Voice &voice = m_voices[iVoice];

		// No voice reset, this function should initialize all necessary components
		// and be able to use previous values such as oscillator phase to enable/disable

		// Voice not sustained
		voice.m_sustained = false;

		const unsigned key = request.key;        // Key
		const float jitter = m_patch.jitter;     // Jitter
		const float velocity = request.velocity; // Velocity

		// Store key & velocity immediately (used by CalcOpIndex())
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
		
		// Initialize LFO
		const float phaseAdj = (true == m_patch.lfoKeySync)
			? 0.f // Synchronized 
			: m_globalLFO->Get(); // Adopt running phase

		const float phaseJitter = CalcPhaseJitter(jitter);
		voice.m_LFO.Initialize(Oscillator::kSine, m_globalLFO->GetFrequency(), m_sampleRate, phaseAdj+phaseJitter, 0.f); // FIXME: more forms!

		// Get dry FM patch		
		FM_Patch &patch = m_patch.patch;

		// Default glide (in case frequency is manipulated whilst playing)
		voice.freqGlide = kDefPolyFreqGlide;

		// Envelope velocity scaling: higher velocity *can* mean longer decay phase
		// This is specifically designed for piano, guitar et cetera
		const float envVelScaling = 1.f + powf(velocity, 2.f)*m_patch.velocityScaling;

		// Set up voice operators
		for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
		{
			const FM_Patch::Operator &patchOp = patch.operators[iOp];
			Voice::Operator &voiceOp = voice.m_operators[iOp];

			voiceOp.enabled   = patchOp.enabled;
			voiceOp.isCarrier = patchOp.isCarrier;

			if (true == voiceOp.enabled)
			{
				// Operator velocity
				const float opVelocity = (false == patchOp.velocityInvert) ? velocity : 1.f-velocity;

				// (Re)set constant/static filter
				voiceOp.filterSVF.resetState();

				// Calculate (scaled) cutoff freq. & Q
				const float cutoffNorm = lerpf<float>(patchOp.cutoff, 1.f, CalcCutoffScaling(key, patchOp));
				const float cutoffHz   = CutoffToHz(cutoffNorm, m_Nyquist);
				const float Q          = ResoToQ(patchOp.resonance);
				
				switch (patchOp.filterType)
				{
				case FM_Patch::Operator::kNoFilter:
					// FIXME: this just passes everything through, but I should perhaps add a full pass-through to the SVF impl.
					voiceOp.filterSVF.updateCoefficients(CutoffToHz(1.f, m_Nyquist), ResoToQ(0.f), SvfLinearTrapOptimised2::LOW_PASS_FILTER, m_sampleRate);
					break;

				case FM_Patch::Operator::kLowpassFilter:
					voiceOp.filterSVF.updateCoefficients(cutoffHz, Q, SvfLinearTrapOptimised2::LOW_PASS_FILTER, m_sampleRate);
					break;

				case FM_Patch::Operator::kHighpassFilter:
					voiceOp.filterSVF.updateCoefficients(cutoffHz, Q, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
					break;

				case FM_Patch::Operator::kBandpassFilter:
					voiceOp.filterSVF.updateCoefficients(cutoffHz, Q, SvfLinearTrapOptimised2::BAND_PASS_FILTER, m_sampleRate);
					break;
						
				default:
					SFM_ASSERT(false);
				}
	
				const float frequency = CalcOpFreq(fundamentalFreq, patchOp);
				const float amplitude = CalcOpIndex(key, opVelocity, patchOp);

				// Start oscillator
				const float phaseShift = (true == patchOp.keySync)
					? 0.f // Synchronized
					: voiceOp.oscillator.GetPhase(); // Running

				voiceOp.oscillator.Initialize(
					patchOp.waveform, frequency, m_sampleRate, phaseShift, 0.f);

				// Set static amplitude
				voiceOp.amplitude.SetRate(m_sampleRate, kDefParameterLatency);
				voiceOp.amplitude.Set(amplitude);
					
				// No interpolation
				voiceOp.curFreq.SetRate(m_sampleRate, kDefPolyFreqGlide);
				voiceOp.curFreq.Set(frequency);
				voiceOp.setFrequency = frequency;

				// Envelope key scaling: higher note (key) means shorter envelope
				const float envKeyScaling = 1.f - 0.9f*CalcKeyScaling(key, patchOp);
				
				// Start envelope
				voiceOp.envelope.Start(patchOp.envParams, m_sampleRate, patchOp.isCarrier, envKeyScaling, envVelScaling, 0.f); 

				// Modulation/Feedback sources
				voiceOp.modulators[0] = patchOp.modulators[0];
				voiceOp.modulators[1] = patchOp.modulators[1];
				voiceOp.modulators[2] = patchOp.modulators[2];

				// Feedback
				voiceOp.iFeedback = patchOp.feedback;
				voiceOp.feedbackAmt = { patchOp.feedbackAmt, m_sampleRate, kDefParameterLatency };
				
				// LFO influence
				voiceOp.ampMod   = patchOp.ampMod;
				voiceOp.pitchMod = patchOp.pitchMod;
				voiceOp.panMod   = patchOp.panMod;

				// Panning
				voiceOp.panAngle = { CalcPanningAngle(patchOp), m_sampleRate, kDefParameterLatency };

				// Distortion
				voiceOp.drive = { patchOp.drive, m_sampleRate, kDefParameterLatency };
			}
		}

		// Reset filters
//		voice.m_filterSVF1.setGain(3.0);
//		voice.m_filterSVF2.setGain(3.0);
		voice.m_filterSVF1.resetState();
		voice.m_filterSVF2.resetState();

		// Start filter envelope
		voice.m_filterEnvelope.Start(m_patch.filterEnvParams, m_sampleRate, false, 1.f, envVelScaling, 0.f);

		// Start pitch envelope
		voice.m_pitchEnvelope.Start(m_patch.pitchEnvParams, m_sampleRate, m_patch.pitchBendRange);

		// Voice is now playing
		voice.m_state = Voice::kPlaying;
		++m_voiceCount;

		// Store (new) index in key slot
		SFM_ASSERT(-1 == GetVoice(key));
		SetKey(key, iVoice);
	}

	// Specialized function for monophonic voices
	// Not ideal due to code, quite a bit of, duplication, but stashing this in one function would be much harder to follow
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

		// Store key & velocity immediately (used by CalcOpIndex())
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
			const float phaseAdj = (true == m_patch.lfoKeySync) 
				? 0.f // Synchronized 
				: m_globalLFO->Get(); // Adopt running phase

			const float phaseJitter = CalcPhaseJitter(jitter);
			voice.m_LFO.Initialize(Oscillator::kSine, m_globalLFO->GetFrequency(), m_sampleRate, phaseAdj+phaseJitter, 0.f); // FIXME: more forms!
		}

		// Envelope velocity scaling: higher velocity *can* mean longer decay phase
		// This is specifically designed for piano, guitar et cetera
		const float envVelScaling = 1.f + powf(velocity, 2.f)*m_patch.velocityScaling;

		// Get dry FM patch		
		FM_Patch &patch = m_patch.patch;

		// Calc. attenuated glide (using new velocity, feels more natural)
		float monoGlide = m_patch.monoGlide;
		float glideAtt = 1.f - m_patch.monoAtt*request.velocity;
		voice.freqGlide = monoGlide*glideAtt;

		// Set up voice operators
		for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
		{
			const FM_Patch::Operator &patchOp = patch.operators[iOp];
			Voice::Operator &voiceOp = voice.m_operators[iOp];

			voiceOp.enabled   = patchOp.enabled;
			voiceOp.isCarrier = patchOp.isCarrier;

			if (true == voiceOp.enabled)
			{
				// Operator velocity
				const float opVelocity = (false == patchOp.velocityInvert) ? velocity : 1.f-velocity;

				if (true == reset)
				{
					// (Re)set constant/static filter
					voiceOp.filterSVF.resetState();
				
					// Calculate (scaled) cutoff freq. & Q
					const float cutoffNorm = lerpf<float>(patchOp.cutoff, 1.f, CalcCutoffScaling(key, patchOp));
					const float cutoffHz   = CutoffToHz(cutoffNorm, m_Nyquist);
					const float Q          = ResoToQ(patchOp.resonance*kDefFilterResonanceLimit /* Lesser range in favor of better control sensitivity */);
				
					switch (patchOp.filterType)
					{
					case FM_Patch::Operator::kNoFilter:
						// FIXME: this just passes everything through, but I should perhaps add a full pass-through to the SVF impl.
						voiceOp.filterSVF.updateCoefficients(CutoffToHz(1.f, m_Nyquist), ResoToQ(0.f), SvfLinearTrapOptimised2::LOW_PASS_FILTER, m_sampleRate);
						break;

					case FM_Patch::Operator::kLowpassFilter:
						voiceOp.filterSVF.updateCoefficients(cutoffHz, Q, SvfLinearTrapOptimised2::LOW_PASS_FILTER, m_sampleRate);
						break;

					case FM_Patch::Operator::kHighpassFilter:
						voiceOp.filterSVF.updateCoefficients(cutoffHz, Q, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
						break;

					case FM_Patch::Operator::kBandpassFilter:
						voiceOp.filterSVF.updateCoefficients(cutoffHz, Q, SvfLinearTrapOptimised2::BAND_PASS_FILTER, m_sampleRate);
						break;
						
					default:
						SFM_ASSERT(false);
					}
				}
				
				const float frequency = CalcOpFreq(fundamentalFreq, patchOp);
				const float amplitude = CalcOpIndex(key, opVelocity, patchOp);

				const float curFreq   = voiceOp.oscillator.GetFrequency();
//				const float curPhase  = voiceOp.oscillator.GetPhase(); 
//				const float curAmp    = voiceOp.amplitude.Get();

				if (true == reset)
				{
					// Reset
					voiceOp.oscillator.Initialize(
						patchOp.waveform, frequency, m_sampleRate, 0.f, 0.f);
	
					voiceOp.amplitude.SetRate(m_sampleRate, voice.freqGlide);
					voiceOp.amplitude.Set(amplitude);

					voiceOp.curFreq.SetRate(m_sampleRate, voice.freqGlide);
					voiceOp.curFreq.Set(frequency);

					const float envKeyScaling = 1.f - 0.9f*CalcKeyScaling(key, patchOp);
					voiceOp.envelope.Start(patchOp.envParams, m_sampleRate, patchOp.isCarrier, envKeyScaling, envVelScaling, 0.f); 
				}
				else
				{
					// Glide
//					voiceOp.amplitude.Set(curAmp);
					voiceOp.amplitude.SetTarget(amplitude);

//					voiceOp.curFreq.Set(curFreq);
					voiceOp.curFreq.SetTarget(frequency);
				}

				voiceOp.setFrequency = frequency;

				// Modulation/Feedback sources
				voiceOp.modulators[0] = patchOp.modulators[0];
				voiceOp.modulators[1] = patchOp.modulators[1];
				voiceOp.modulators[2] = patchOp.modulators[2];

				// Feedback
				voiceOp.iFeedback = patchOp.feedback;
				voiceOp.feedbackAmt = { patchOp.feedbackAmt, m_sampleRate, kDefParameterLatency };
				
				// LFO influence
				voiceOp.ampMod   = patchOp.ampMod;
				voiceOp.pitchMod = patchOp.pitchMod;
				voiceOp.panMod   = patchOp.panMod;

				// Panning
				voiceOp.panAngle = { CalcPanningAngle(patchOp), m_sampleRate, kDefParameterLatency };

				// Distortion
				voiceOp.drive = { patchOp.drive, m_sampleRate, kDefParameterLatency };
			}
		}

		if (true == reset)
		{
			// Reset filters
			voice.m_filterSVF1.resetState();
			voice.m_filterSVF2.resetState();

			// Start filter envelope
			voice.m_filterEnvelope.Start(m_patch.filterEnvParams, m_sampleRate, false, 1.f, envVelScaling, 0.f);

			// Start pitch envelope
			voice.m_pitchEnvelope.Start(m_patch.pitchEnvParams, m_sampleRate, m_patch.pitchBendRange);
		}

		// Voice is now playing
		voice.m_state = Voice::kPlaying;
		++m_voiceCount;

		// Store (new) index in key slot
		SFM_ASSERT(-1 == GetVoice(key));
		SetKey(key, 0);
	}

	/* ----------------------------------------------------------------------------------------------------

		Voice logic handling, (to be) called during Render().

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

			// Steal *all* active voices
			for (unsigned iVoice = 0; iVoice < kMaxVoices; ++iVoice)
			{
				Voice &voice = m_voices[iVoice];
				
				if (false == voice.IsIdle() && false == voice.IsStolen())
				{
					StealVoice(iVoice);

//					Log("Voice mode switch, stealing voice: " + std::to_string(iVoice));
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

		if (false == monophonic)
		{
			/*
				Handle all release requests (polyphonic)
			*/

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
			Update voices
		*/

		for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
		{
			Voice &voice = m_voices[iVoice];
			{
				// Active?
				if (false == voice.IsIdle())
				{
					// Fully released and ready for cooldown?
					if (false == voice.IsDone() && false == voice.IsStolen()) // No reason to further update a stolen voice
					{
						if (-1 != voice.m_key)
						{
							// Update active voices
							// Each (active) operator has a set of parameters that need per-sample interpolation 
							// Some of these are updated in this loop
							// The set of parameters (also outside of this object) isn't conclusive and may vary depending on the use of FM. BISON (currently customized for VST plug-in)
					
							const float freqGlide = voice.freqGlide;						

							for (unsigned iOp = 0; iOp < kNumOperators; ++iOp)
							{
								auto &voiceOp = voice.m_operators[iOp];

								// Update per-sample interpolated parameters
								if (true == voiceOp.enabled)
								{
									const float fundamentalFreq = voice.m_fundamentalFreq;
									const FM_Patch::Operator &patchOp = m_patch.patch.operators[iOp];

									// Operator velocity
									const float opVelocity = (false == patchOp.velocityInvert) ? voice.m_velocity : 1.f-voice.m_velocity;

									const float frequency = CalcOpFreq(fundamentalFreq, patchOp);
									const float amplitude = CalcOpIndex(voice.m_key, opVelocity, patchOp);
								
									// Interpolate if necessary
									if (frequency != voiceOp.setFrequency)
									{
										voiceOp.curFreq.SetTarget(frequency);
										voiceOp.setFrequency = frequency;
									}

									// Amplitude (output level or "index")
									voiceOp.amplitude.SetTarget(amplitude);

									// Distortion
									voiceOp.drive.SetTarget(patchOp.drive);

									// Feedback amount
									voiceOp.feedbackAmt.SetTarget(patchOp.feedbackAmt);
					
									// Panning (as set by static parameter)
									voiceOp.panAngle.SetTarget(CalcPanningAngle(patchOp));
								}
							}
						}
						else
						{
							// If key is -1, this voice is the tail end of a note that has been retriggered; I don't feel it's warranted to add yet
							// another variable to handle the update, nor do I think it's necessary to sound right.
							SFM_ASSERT(true == voice.IsReleasing());
						}
					}
				}
			}
		}
		
		/*
			Simple first-fit voice allocation (if none available stealing will be attempted in UpdateVoicesPostRender())
		*/

		if (false == monophonic)
		{
			/* Polyphonic */

			// Sort list by time stamp
			std::sort(m_voiceReq.begin(), m_voiceReq.end(), [](const auto &left, const auto &right ) -> bool { return left.timeStamp > right.timeStamp; } );

			while (m_voiceReq.size() > 0 && m_voiceCount < m_curPolyphony)
			{
				// Pick first free voice
				for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
				{
					Voice &voice = m_voices[iVoice];

					if (true == voice.IsIdle())
					{
						InitializeVoice(iVoice);
						break;
					}
				}
			}
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
		}
	}

	// Update voices after Render() pass
	void Bison::UpdateVoicesPostRender()
	{
		/*
			Free (stolen) voices
		*/

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

		const bool monophonic = Patch::VoiceMode::kMono == m_curVoiceMode;

		if (true == monophonic)
		{
			SFM_ASSERT(true == m_voiceReq.empty());
			
			// In monophonic mode we won't need to steal any voices at this point
			return;
		}
	
		/*
			Try to steal the required amount of voices
			
			For now if voices are stolen there's 1 Render() call (or frame if you will) delay before the new voice(s) are triggered,
			this avoids clicks at the cost of minimal latency

			FIXME: here's some proper discussion on the matter: https://www.kvraudio.com/forum/viewtopic.php?t=91557
		*/

		size_t voicesNeeded = m_voiceReq.size();
		size_t voicesStolen = 0;

		while (voicesNeeded--)
		{
			float lowestOutput = 1.f*kNumOperators;
			int iLowest = -1;
			
			// FIXME: repeating this loop over and over is a little ham-fisted, is it not?
			for (unsigned iVoice = 0; iVoice < m_curPolyphony; ++iVoice)
			{
				Voice &voice = m_voices[iVoice];
				
				const bool isIdle      = voice.IsIdle(); 
//				const bool isPlaying   = voice.IsPlaying();
				const bool isReleasing = voice.IsReleasing();
				const bool isStolen    = voice.IsStolen();

				// Bias releasing voices
				const float releaseScale = (true == isReleasing) ? kVoiceStealReleaseBias : 1.f;

				if (false == isIdle && false == isStolen)
				{
					// Check output level
					const float output = voice.GetSummedOutput()*releaseScale;
					if (lowestOutput > output)
					{
						// Lowest so far
						iLowest = iVoice;
						lowestOutput = output;
					}
				}
			}
			
			// Found one?
			if (-1 != iLowest)
			{
				// Steal it
				StealVoice(iLowest);
				Log("Voice stolen (index): "  + std::to_string(iLowest));

				++voicesStolen;
			}
			else break; // Nothing to steal at all, so bail
		}

		if (false == m_voiceReq.empty())
		{
			// Report any voices we can't immediately trigger next frame
			if (voicesStolen < m_voiceReq.size())
				Log("Voice requests deferred: " + std::to_string(m_voiceReq.size()));
		}
	}

	// Update sustain state
	void Bison::UpdateSustain()
	{
		if (Patch::kNoPedal == m_patch.sustainType)
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
						
						// Issue NOTE_OFF
//						NoteOff(voice.m_key, 0);

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

			const double falloff = m_patch.pianoPedalFalloff;
			const double relMul  = m_patch.pianoPedalReleaseMul;
			
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
						voice.m_filterEnvelope.OnPianoSustain(m_sampleRate, falloff, relMul);

						for (auto& voiceOp : voice.m_operators)
							if (true == voiceOp.enabled && true == voiceOp.isCarrier)
							{
								voiceOp.envelope.OnPianoSustain(m_sampleRate, falloff, relMul);
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
						// Voice is no longer sustained, so it can be released
						voice.m_sustained = false;
						
						voice.m_filterEnvelope.OnPianoSustain(m_sampleRate, falloff, relMul);

						for (auto& voiceOp : voice.m_operators)
							if (true == voiceOp.enabled && true == voiceOp.isCarrier)
							{
								voiceOp.envelope.OnPianoSustain(m_sampleRate, falloff, relMul);
							}
						
						// Issue NOTE_OFF
//						NoteOff(voice.m_key, 0);

						Log("Voice no longer sustained (CP): " + std::to_string(iVoice));
					}
				}
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
		SFM_ASSERT(nullptr != m_pBufL && nullptr != m_pBufR);

		if (numSamples > m_samplesPerBlock)
		{
			// JUCE didn't keep it's promise, or so I think
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

		// Calculate current BPM freq.
		// FIXME: move to SetBPM()
		if (true == m_patch.beatSync && 0.f != m_BPM)
		{
			const float ratio = m_patch.beatSyncRatio; // Note ratio
			SFM_ASSERT(ratio >= 0);

			const float BPM = float(m_BPM);
			const float BPS = float(BPM)/60.f;    // Beats per sec.
			m_freqBPM = BPS/ratio;                // Sync. freq.
		}
		else
			// None: interpret this as a cue to use user controlled rate(s)
			m_freqBPM = 0.f;

		// Set LFO freq.
		float freqLFO = 0.f;

		if (false == m_patch.beatSync || m_freqBPM == 0.f)
		{
			// Set LFO speed in (DX7) range
			freqLFO = MIDI_To_LFO_Hz(m_patch.lfoRate /* I could not spot any artifacts, so we'll go without interpolation for now! */);
			m_globalLFO->SetFrequency(freqLFO);
		}
		else
		{
			// Adapt BPM freq.
			freqLFO = m_freqBPM;

			if (false == m_resetPhaseBPM)
			{
				m_globalLFO->SetFrequency(freqLFO);
			}
			else
			{
				// Full reset; likely to be used when (re)starting a track
				// This *must* be done prior to UpdateVoicesPreRender()
				m_globalLFO->Initialize(freqLFO, m_sampleRate);
			}
		}

		// Update voice logic
		UpdateVoicesPreRender();

		// Update filter type & state
		// This is where the magic happens ;)

		SvfLinearTrapOptimised2::FLT_TYPE filterType1; // Actually the *second* step if 'secondFilterpass' is true, and the only if it's false
		SvfLinearTrapOptimised2::FLT_TYPE filterType2 = SvfLinearTrapOptimised2::NO_FLT_TYPE;
		
		SFM_ASSERT(16.f >= kMaxFilterResonance);
		
		// Set target cutoff (Hz) & Q
		const float normCutoff = m_cutoffPF.Apply(m_patch.cutoff);
		const float resonance = m_resoPF.Apply(m_patch.resonance);

		// Using smoothstepf() to add a little curvature, chiefly intended to appease basic MIDI controls
		/* const */ float cutoff = CutoffToHz(smoothstepf(normCutoff), m_Nyquist);
		/* const */ float Q = ResoToQ(smoothstepf(resonance*m_patch.resonanceLimit));
		
		m_curCutoff.SetTarget(cutoff);
		m_curQ.SetTarget(Q);
		
		// Set filter type & parameters
		bool secondFilterPass = false;      // Second pass to BISON-ize the filter
		float secondQOffs = 0.f;            // Offset on Q for second BISON stage
		float qDiv = 1.f;                   // Divider on Q to tame the resonance range (main stage only, tweaked for 16!)
		float fullCutoff;                   // Full cutoff used to apply VCF
//		bool isLowAndHigh = false;          // Used to limit LOW_PASS_FILTER range

		switch (m_patch.filterType)
		{
		default:
		case Patch::kNoFilter:
			filterType1 = SvfLinearTrapOptimised2::NO_FLT_TYPE;
			fullCutoff = CutoffToHz(1.f, m_Nyquist);
			break;

		case Patch::kLowpassFilter:
			// Screams and yells
			secondFilterPass = true;
			secondQOffs = 0.1f;
			filterType1 = SvfLinearTrapOptimised2::LOW_PASS_FILTER;
			filterType2 = SvfLinearTrapOptimised2::LOW_PASS_FILTER;
			fullCutoff = CutoffToHz(1.f, m_Nyquist);
			break;

		case Patch::kHighpassFilter:
			filterType1 = SvfLinearTrapOptimised2::HIGH_PASS_FILTER;
			fullCutoff = CutoffToHz(0.f, m_Nyquist);
			break;

		case Patch::kBandpassFilter:
			// Pretty standard with a reduced resonance range
			qDiv *= 0.25f;
			filterType1 = SvfLinearTrapOptimised2::BAND_PASS_FILTER;
			fullCutoff = CutoffToHz(1.f, m_Nyquist);
			break;
		}

		// Switched filter type?
		const bool resetFilter = m_curFilterType != filterType1;
		m_curFilterType = filterType1;

		// Set pitch & amp. wheel & modulation target values
		const float bendWheelFiltered = m_bendWheelPF.Apply(bendWheel);
		if (false == m_patch.pitchIsAmpMod)
		{
			// Wheel modulates pitch
			m_curPitchBend.SetTarget(bendWheelFiltered);
			m_curAmpBend.SetTarget(0.f);
		}
		else
		{
			// Wheel modulates amplitude
			m_curAmpBend.SetTarget(bendWheelFiltered);
			m_curPitchBend.SetTarget(0.f);
		}


		// Set modulation & aftertouch target values
		m_curModulation.SetTarget(m_modulationPF.Apply(modulation));

		const float aftertouchFiltered = m_aftertouchPF.Apply(aftertouch);
		m_curAftertouch.SetTarget(aftertouchFiltered);
		
		// Start rendering voices, if necessary
		const unsigned numVoices = m_voiceCount;

		// Clear L/R buffers
		memset(m_pBufL, 0, m_samplesPerBlock*sizeof(float));
		memset(m_pBufR, 0, m_samplesPerBlock*sizeof(float));

		float avgVelocity = 0.f;

		if (0 != numVoices)
		{
			float left = 0.f, right = 0.f;
			
			// Swap 2 branches for multiplications later on
			const float mainFilterAftertouch = (Patch::kMainFilter == m_patch.aftertouchMod) ? 1.f : 0.f;
			const float modulationAftertouch = (Patch::kModulation == m_patch.aftertouchMod) ? 1.f : 0.f;
			
			// Global per-voice gain
			const float voiceGain = 0.354813397f; // dBToGain(kVoiceGaindB);
		
			// Render voices
			for (int iVoice = 0; iVoice < kMaxVoices /* Actual voice count can be > m_curPolyphony due to being stolen on ResetVoices() */; ++iVoice)
			{
				Voice &voice = m_voices[iVoice];

				voice.m_LFO.SetFrequency(freqLFO);
				
				if (false == voice.IsIdle())
				{
					const float globalAmpFull = 1.f;
					InterpolatedParameter<kLinInterpolate> globalAmp(globalAmpFull, std::min<unsigned>(128, numSamples));

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
							globalAmp.SetTarget(globalAmpFull);
						}

						// Add to avgVelocity
						if (false == voice.IsReleasing())
							avgVelocity += voice.m_velocity;
					}
	
					if (true == resetFilter)
					{
						// Reset
						voice.m_filterSVF1.resetState();
						voice.m_filterSVF2.resetState();
					}

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

					const bool noFilter = SvfLinearTrapOptimised2::NO_FLT_TYPE == filterType1;
					auto& filterEG      = voice.m_filterEnvelope;

					// Second filter lags behind a little to add a bit of "gritty sparkle"
					LowpassFilter secondCutoffLPF(5000.f/m_sampleRate);
					secondCutoffLPF.Reset(curCutoff.Get());
					
					for (unsigned iSample = 0; iSample < numSamples; ++iSample)
					{
						const float sampAftertouch = curAftertouch.Sample();

						// FIXME: 
						// - Break this up in smaller loops writing to (sequential) buffers, and lastly store the result,
						//   as this just can't be fast enough for production quality (release); also keep in mind that
						//   there might come a point where we might want to render voices in chunks, to quantize MIDI
						//   events for more accurate playback
						// - 31/03: do this in VST?
						// - Test if there are still any over- and/or undershoots
						// - Should I use smoothstepf() for aftertouch all over?
						//   When we regard this engine as an actual instrument: *yes*

						const float sampMod = std::min<float>(1.f, curModulation.Sample() + smoothstepf(modulationAftertouch*sampAftertouch));
						
						// Render dry voice
						voice.Sample(
							left, right, 
							powf(2.f, curPitchBend.Sample()*(m_patch.pitchBendRange/12.f)),
							curAmpBend.Sample()+1.f, // [0.0..2.0]
							sampMod);

						// Sample filter envelope
						float filterEnv = filterEG.Sample();
						if (true == m_patch.filterEnvInvert)
							filterEnv = 1.f-filterEnv;

						// SVF cutoff aftertouch (curved towards zero if pressed)
						const float cutAfter = mainFilterAftertouch*sampAftertouch;
						SFM_ASSERT(cutAfter >= 0.f && cutAfter <= 1.f);
						
						// Apply & mix filter (FIXME: write single sequential loop (see Github issue), prepare buffer(s) on initialization)
						if (false == noFilter)
						{	
							float filteredL = left;
							float filteredR = right;
							
							// Cutoff & Q, finally, for *this* sample
							/* const */ float sampCutoff = curCutoff.Sample()*(1.f - smoothstepf(cutAfter*kMainCutoffAftertouchRange));
							const float sampQ = curQ.Sample()*qDiv;
							
							// Add some sparkle?
							// Cascade!
							if (true == secondFilterPass)
							{
								SFM_ASSERT(SvfLinearTrapOptimised2::NO_FLT_TYPE != filterType2);

								const float secondCutoff = lerpf<float>(fullCutoff, secondCutoffLPF.Apply(sampCutoff), filterEnv); 
								const float secondQ      = std::min<float>(kMaxFilterResonance, sampQ+secondQOffs);
								voice.m_filterSVF2.updateCoefficients(secondCutoff, secondQ, filterType2, m_sampleRate);
								voice.m_filterSVF2.tick(filteredL, filteredR);
							}

							// Ref: https://github.com/FredAntonCorvest/Common-DSP/blob/master/Filter/SvfLinearTrapOptimised2Demo.cpp
							voice.m_filterSVF1.updateCoefficients(lerpf<float>(fullCutoff, sampCutoff, filterEnv), sampQ, filterType1, m_sampleRate);
							voice.m_filterSVF1.tick(filteredL, filteredR);
							
							left  = filteredL;
							right = filteredR;
						}
						
						// Apply gain and add to mix
						const float amplitude = curGlobalAmp.Sample() * voiceGain;
						m_pBufL[iSample] += amplitude * left;
						m_pBufR[iSample] += amplitude * right;
					}
				}
			}
			
			// Normalize
			avgVelocity /= numVoices;
			SFM_ASSERT(avgVelocity <= 1.f);
		}
				
		// Update voice logic
		UpdateVoicesPostRender();

		// Update sustain state
		UpdateSustain();

		// Advance global LFO phase
		m_globalLFO->Ticks(numSamples);

		// Calculate post-pass filter cutoff freq.
		const float postNormCutoff = m_postCutoffPF.Apply(m_patch.postCutoff);
		const float postCutoffHz = CutoffToHz(postNormCutoff, m_Nyquist/2 /* Nice range; check synth-post-pass.cpp for max. range */, 0.f);

		// Feed either a flat or aftertouch parameter to the post-pass to define the filter's wetness
		float postWet = m_patch.postWet;
		if (Patch::kPostFilter == m_patch.aftertouchMod)
			postWet = std::min<float>(1.f, postWet+aftertouchFiltered);

		// Apply post-processing (FIXME: pass structure!)
		m_postPass->Apply(numSamples, 
		                  /* BPM sync. */
						  m_freqBPM,
		                  /* Chorus/Phaser */
						  m_effectRatePF.Apply(m_patch.cpRate), 
						  m_effectWetPF.Apply(m_patch.cpWet), 
						  false == m_patch.cpIsPhaser,
						  /* Delay */
						  m_delayPF.Apply(m_patch.delayInSec),
						  m_delayWetPF.Apply(m_patch.delayWet),
						  m_delayFeedbackPF.Apply(m_patch.delayFeedback),
						  m_delayFeedbackCutoffPF.Apply(m_patch.delayFeedbackCutoff),
						  /* MOOG-style 24dB filter + Tube amp. distort */
		                  postCutoffHz, // Filtered above
						  m_postResoPF.Apply(m_patch.postResonance),
						  m_postDrivePF.Apply(m_patch.postDrivedB),
		                  m_postWetPF.Apply(postWet),
						  m_avgVelocityPF.Apply(avgVelocity),
						  m_tubeDistPF.Apply(m_patch.tubeDistort),
						  m_patch.tubeDrivedB, // FIXME: filter?
						  /* Reverb */
						  m_reverbWetPF.Apply(m_patch.reverbWet),
						  m_reverbRoomSizePF.Apply(m_patch.reverbRoomSize),
						  m_reverbDampeningPF.Apply(m_patch.reverbDampening),
						  m_reverbWidthPF.Apply(m_patch.reverbWidth),
						  m_reverbLP_PF.Apply(m_patch.reverbLP),
						  m_reverbHP_PF.Apply(m_patch.reverbHP),
						  m_reverbPreDelayPF.Apply(m_patch.reverbPreDelay),
						  /* Compressor (FIXME: apply ParameterFilter if audibly necessary) */
						  m_patch.compPeakToRMS,
						  m_patch.compThresholddB,
						  m_patch.compKneedB,
						  m_patch.compRatio,
						  m_patch.compGaindB,
						  m_patch.compAttack,
						  m_patch.compRelease,
						  m_compLookaheadPF.Apply(m_patch.compLookahead),
						  /* Master volume */
						  m_masterVolPF.Apply(m_patch.masterVol),
						  /* Buffers */
						  m_pBufL, m_pBufR, pLeft, pRight);

		// Of all these, copies were used per voice, so let their parent skip numSamples to keep up	
		m_curCutoff.Skip(numSamples);
		m_curQ.Skip(numSamples);
		m_curPitchBend.Skip(numSamples);
		m_curAmpBend.Skip(numSamples);
		m_curModulation.Skip(numSamples);
		m_curAftertouch.Skip(numSamples);

		// This has been done by now
		m_resetVoices   = false;
		m_resetPhaseBPM = false;
	}

}; // namespace SFM
