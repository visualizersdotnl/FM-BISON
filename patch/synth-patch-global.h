
/*
	FM. BISON hybrid FM synthesis -- Patch globals.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	This is the parent patch, which contains a set of our FM operators and all parameters for global
	features.

	In case a parameter does not come with a comment it can be most likely be normalized ([0..1]) range,
	though in some times it helps to look at synth-global.h; it *should* be documented here so it's a FIXME.

	Nearly all of these parameters are interpolated per sample.
*/

#pragma once

#include "../synth-global.h"

#include "synth-patch-operators.h"
#include "../synth-pitch-envelope.h"

namespace SFM
{
	// BPM sync. mode note ratios, adopted from the Arturia Keystep
	constexpr size_t kNumBeatSyncRatios = 12;

	static const float kBeatSyncRatios[kNumBeatSyncRatios]
	{
		4.f,            // 1/1
		2.6666666667f,  // 1/1T
		2.f,            // 1/2
		1.3333333336f,  // 1/2T
		1.f,            // 1/4
		0.6666666668f,  // 1/4T
		0.5f,           // 1/8
		0.3333333334f,  // 1/8T
		0.25f,          // 1/16
		0.1666666667f,  // 1/16T
		0.125f,         // 1/32
		0.08333333335f, // 1/32T 	
	};

	constexpr unsigned kNumLFOWaveforms = 10;

	const Oscillator::Waveform kLFOWaveforms[kNumLFOWaveforms] =
	{
			Oscillator::Waveform::kNone,
			Oscillator::Waveform::kSine, 
			Oscillator::Waveform::kPolyTriangle,
			Oscillator::Waveform::kBump,
			Oscillator::Waveform::kSoftSaw,
			Oscillator::Waveform::kSoftRamp,
			Oscillator::Waveform::kPolySaw,
			Oscillator::Waveform::kPolyRamp,
			Oscillator::Waveform::kPolyRectangle,
			Oscillator::Waveform::kSampleAndHold
	};

	struct Patch
	{
		// FM operators
		PatchOperators operators;
		
		// Voice mode
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
		Oscillator::Waveform LFOWaveform1; // Waveform A
		Oscillator::Waveform LFOWaveform2; // Waveform B
		Oscillator::Waveform LFOWaveform3; // Mod. waveform
		float LFOBlend;
		int   LFOModSpeed; // [kMinLFOModSpeed..kMaxLFOModSpeed]
		float LFOModDepth; 
		float LFORate; // [0.0..100.0]
		bool  LFOKeySync;
		float modulationOverride; // If non-zero overrides Render() modulation parameter, which is typically the mod. wheel (MIDI)

		// S&H
		float SandHSlewRate;

		// BPM sync. mode (LFO, Chorus, Phaser, Auto-wah, Delay, ...)
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

		// Auto-wah settings (see synth-global.h & synth-auto-wah.h for non-normalized parameters)
		float wahResonance;
		float wahAttack;
		float wahHold;
		float wahRate;
		float wahDrivedB; // [kMinWahDrivedB..kMaxWahDrivedB]
		float wahSpeak;
		float wahSpeakVowel; // [0..kMaxWahSpeakVowel]
		float wahSpeakVowelMod;
		float wahSpeakGhost;
		float wahSpeakCut;
		float wahSpeakResonance;
		float wahCut;
		float wahWet;
		
		// Reverb settings
		float reverbWet;
		float reverbRoomSize;
		float reverbDampening;
		float reverbWidth;
		float reverbPreDelay;
		float reverbLP; // Reverb LP and HP must be 1 for complete pass-through
		float reverbHP; //

		// Compressor settings (see synth-global.h & synth-compressor.h for non-normalized parameters)
		float compThresholddB;
		float compKneedB;
		float compRatio;
		float compGaindB;
		float compAttack;
		float compRelease;
		float compLookahead;
		bool compAutoGain;
		float compRMSToPeak;

		// Filter parameters
		enum FilterType
		{
			kNoFilter,
			kLowpassFilter,
			kHighpassFilter,
			kBandpassFilter,
			kNumFilters
		} filterType;

		float cutoff;
		float resonance;
		float resonanceLimit;
		
		// Post-pass 24dB MOOG-style ladder filter
		float postCutoff; // Up to 1/4th of Nyquist for now (see Render())
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

		// Tube distortion: amount & drive (drives cubic distortion)
		float tubeDistort;
		float tubeDrive;   // [kMinTubeDrive..kMaxTubeDrive]
		float tubeOffset;  // [kMinTubeOffset..kMaxTubeOffset]

		// Piano pedal
		float pianoPedalFalloff;
		float pianoPedalReleaseMul;

		// Acoustic scaling (more velocity means longer decay phase; useful, mostly, for acoustic instruments)
		float acousticScaling;

		void ResetToEngineDefaults()
		{
			// Reset patch
			operators.ResetToEngineDefaults();
			
			// Polyphonic
			voiceMode = kPoly;
			monoGlide = kDefMonoFreqGlide;
			monoAtt   = kDefMonoGlideAtt;

			// Def. master vol.
			masterVol = kDefVolumedB;

			// Def. bend range
			pitchBendRange = kDefPitchBendRange;

			// LFO
			LFOWaveform1 = kLFOWaveforms[1]; // Sine
			LFOWaveform2 = kLFOWaveforms[1]; // Sine
			LFOWaveform3 = kLFOWaveforms[0]; // None
			LFOBlend = 0.f;                  // Waveform A
			LFOModSpeed = 0;                 // Equal speed
			LFOModDepth = 0.f;               // No modulation
			LFORate = 0.f;                   // Zero Hz
			LFOKeySync = false;              // No key sync.
			modulationOverride = 0.f;        // Wheel input

			// S&H default(s)
			SandHSlewRate = kDefSandHSlewRate;

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

			// Default auto-wah, 100% dry
			wahResonance = kDefWahResonance;
			wahAttack = kDefWahAttack;
			wahHold = kDefWahHold;
			wahRate = kDefWahRate;
			wahDrivedB = kDefWahDrivedB;
			wahSpeak = 0.f;
			wahSpeakVowel = 0.f; // [0..kMaxWahSpeakVowel]
			wahSpeakVowelMod = 0.f;
			wahSpeakGhost = 0.f;
			wahSpeakCut = 1.f;       // LPF
			wahSpeakResonance = 0.f; //
			wahCut = 0.f;
			wahWet = 0.f;
			
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
			compKneedB = kDefCompKneedB;
			compRatio = kDefCompRatio;
			compGaindB = kDefCompGaindB;
			compAttack = kDefCompAttack;
			compRelease = kDefCompRelease;
			compLookahead = 0.f; // No lookahead
			compAutoGain = true;
			compRMSToPeak = 0.f; // 100% RMS
			
			// Little to no filtering
			filterType = kLowpassFilter;
			cutoff = kDefMainFilterCutoff;
			resonance = kDefMainFilterResonance;
			resonanceLimit = kDefMainFilterResonanceLimit;

			// Post-pass filter (disabled)
			postCutoff = 0.f;
			postResonance = 0.f;
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

			// No tube distortion
			tubeDistort = 0.f;
			tubeDrive = kDefTubeDrive;
			tubeOffset = 0.f;

			// Default piano pedal settings
			pianoPedalFalloff    = kDefPianoPedalFalloff;
			pianoPedalReleaseMul = kDefPianoPedalReleaseMul;

			// No acoustic scaling
			acousticScaling = 0.f;
		}
	};
}
