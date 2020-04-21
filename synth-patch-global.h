
/*
	FM. BISON hybrid FM synthesis -- Patch globals.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	I intend to keep most state [0..1] floating point for easy VST integration.

	For details see synth-global.h!
*/

#pragma once

#include "synth-patch-operators.h"
#include "synth-pitch-envelope.h"

namespace SFM
{
	// BPM sync. mode note ratios, adopted from the Arturia Keystep
	static const float kBeatSyncRatios[8]
	{
		1.0f,           //  1/4
		0.6666666668f,  // 1/4T
		0.5f,           //  1/8
		0.3333333334f,  // 1/8T
		0.25f,          // 1/16
		0.1666666667f,  // 1/16T
		0.125f,         // 1/32
		0.08333333335f, // 1/32T 	
	};

	const size_t kNumBeatSyncRatios = 8;

	struct Patch
	{
		// Dry FM patch
		FM_Patch patch;
		
		// Voice mode (FIXME: implement unison)
		enum VoiceMode
		{
			kPoly,   // Pure polyphony
			kMono,   // Portamento (smooth) monophonic
		} voiceMode;
		
		// Monophonic
		float monoGlide; // Glide in sec.
		float monoAtt;   // Velocity attenuation amount

		// Master volume (in dB)
		float masterVol;

		// Pitch bend range
		int pitchBendRange; // [0..kMaxPitchBendRange]

		// LFO
		float lfoRate;    // Range [0.0..127.0]
		bool  lfoKeySync;  

		// BPM sync. mode (LFO, chorus/phaser, delay, ...)
		bool beatSync;
		float beatSyncRatio; // See kBeatSyncRatios

		// "Analog" jitter (search project to see what it affects and how)
		float jitter;

		// Chorus/Phaser selection, amount & rate [0..1]
		bool cpIsPhaser;
		float cpWet;
		float cpRate;

		// Delay
		float delayInSec;
		float delayWet;
		float delayFeedback;
		float delayFeedbackCutoff;

		// If the pitch wheel should modulate amplitude instead
		bool pitchIsAmpMod;

		// Max. voices for patch
		unsigned maxVoices;
		
		// Reverb settings
		float reverbWet;
		float reverbRoomSize;
		float reverbDampening;
		float reverbWidth;
		float reverbPreDelay;

		// Reverb LP and HP must be 1 for complete pass-through
		float reverbLP;
		float reverbHP;

		// Compressor settings (see synth-global.h for ranges)
		float compThresholddB;
		float compRatio;
		float compAttack;
		float compRelease;

		// Filter parameters
		enum FilterType
		{
			kNoFilter,
			kLowpassFilter,
			kHighpassFilter,
			kBandpassFilter,
			kLowAndHighFilter,
			kNumFilters
		} filterType;

		float cutoff;
		float resonance;
		float resonanceLimit;
		
		// Post-pass 24dB MOOG-style ladder filter
		float postCutoff; // Up to 1/4th of Nyquist for now!
		float postResonance;
		float postDrivedB;
		float postWet;

		// Filter envelope
		Envelope::Parameters filterEnvParams;
		bool filterEnvInvert;

		// Pitch envelope
		PitchEnvelope::Parameters pitchEnvParams;

		// Sustain pedal type
		enum SustainType
		{
			kSynthPedal, // Like the Yamaha DX7
			kPianoPedal, // Like the Yamaha Reface CP
			kNoPedal,    // No sustain
			kNumPedalModes
		} sustainType;

		// Aftertouch modulation target
		enum AftertouchModulationTarget
		{
			kNoAftertouch, // No effect
			kModulation,   // Same effect as modulation (wheel)
			kMainFilter,   // Main filter amount
			kPostFilter,   // Post-pass filter amount
			kNumModTargets
		} aftertouchMod;

		// Tube amp. distortion amount & max. dB (bumps harmonics and soft clips)
		float tubeDistort;
		float tubeDrivedB; // [0..kMaxTubeDrivedB]

		// Piano pedal
		float pianoPedalFalloff;
		float pianoPedalReleaseMul;

		// Velocity scaling
		float velocityScaling;

		void ResetToEngineDefaults()
		{
			// Reset patch
			patch.ResetToEngineDefaults();
			
			// Polyphonic
			voiceMode = kPoly;
			monoGlide = kDefMonoFreqGlide;
			monoAtt   = kDefMonoGlideAtt;

			// Def. master vol.
			masterVol = kDefVolumedB;

			// Def. bend range
			pitchBendRange = kDefPitchBendRange;

			// LFO
			lfoRate = 0.f;      // Zero Hz
			lfoKeySync = false; // No key sync.

			// BPM sync.
			beatSync = false;
			beatSyncRatio = kBeatSyncRatios[0]; // 1/4

			// Zero deviation
			jitter = 0.f;

			// "Silent chorus"
			cpIsPhaser = false;
			cpWet = 0.f;
			cpRate = 0.f;

			// No delay
			delayInSec = 0.f;
			delayWet = 0.f;
			delayFeedback = 0.f;
			delayFeedbackCutoff = 1.f;

			// Pitch wheel affects pitch, not amplitude
			pitchIsAmpMod = false;

			// Def. max voices
			maxVoices = kDefMaxVoices;
			
			// No reverb
			reverbWet = 0.f;
			reverbRoomSize = 0.f;
			reverbDampening = 0.f;
			reverbWidth = kDefReverbWidth;
			reverbLP = 1.f;
			reverbHP = 1.f;
			reverbPreDelay = kDefReverbPreDelay;

			// Default compression
			compThresholddB = kDefCompThresholddB;
			compRatio = kDefCompRatio;
			compAttack = kDefCompAttack;
			compRelease = kDefCompRelease;
			
			// Little to no filtering
			filterType = kLowpassFilter;
			cutoff = kDefFilterCutoff;
			resonance = kDefFilterResonance;
			resonanceLimit = kDefFilterResonanceLimit;

			// Post-pass filter (disabled)
			postCutoff = 0.f;
			postResonance = kDefPostFilterResonance;
			postDrivedB = kDefPostFilterDrivedB;
			postWet = 0.f;

			// Main filter envelope: infinite sustain
			filterEnvParams.preAttack = 0.f;
			filterEnvParams.attack = 0.f;
			filterEnvParams.decay = 0.f;
			filterEnvParams.sustain = 1.f;
			filterEnvParams.release = 1.f; // Infinite!
			filterEnvParams.attackCurve = 0.f;
			filterEnvParams.decayCurve = 0.f;
			filterEnvParams.releaseCurve = 0.f;
			filterEnvParams.rateMul = 1.f; // 1 second

			filterEnvInvert = false;

			// Pitch envelope sounds like a siren :)
			pitchEnvParams.P1 = 1.f;
			pitchEnvParams.P2 = 0.f;
			pitchEnvParams.P3 = -1.f;
			pitchEnvParams.P4 = 0.f;
			pitchEnvParams.R1 = pitchEnvParams.R2 = pitchEnvParams.R3 = 1.f;
			pitchEnvParams.L4 = 0.f;

			// Synthesizer sustain type
			sustainType = kSynthPedal;

			// No aftertouch modulation
			aftertouchMod = kNoAftertouch;

			// No tube amp. distortion
			tubeDistort = 0.f;
			tubeDrivedB = kDefTubeDrivedB;

			// Default piano pedal settings
			pianoPedalFalloff    = kDefPianoPedalFalloff;
			pianoPedalReleaseMul = kDefPianoPedalReleaseMul;

			// No velocity scaling
			velocityScaling = 0.f;
		}
	};
}
