
/*
	FM. BISON hybrid FM synthesis -- Global includes, constants & utility functions.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

// Include JUCE
#include "../JuceLibraryCode/JuceHeader.h"

#ifndef _DEBUG
	#ifdef _WIN32
		#define SFM_INLINE __forceinline // Muscle MSVC into doing what we ask for
	#else
		#define SFM_INLINE __inline // Let inline functions do *not* appear in symbol tables
	#endif
#else
	#define SFM_INLINE inline 
#endif

// _DEBUG should be defined in all debug builds, regardless of platform
// PROFILE_BUILD overrides _DEBUG
// For FM-BISON's internal version I've done this in Projucer

#if defined(_DEBUG) && !defined(PROFILE_BUILD)
	#ifdef _WIN32
		#define SFM_ASSERT(condition) if (!(condition)) __debugbreak();
	#else
		#define SFM_ASSERT(condition) jassert(condition); // FIXME: replace for platform equivalent (see jassert() impl.)
	#endif
#else
	#define SFM_ASSERT(condition)
#endif

// Set to 1 to kill all SFM log output
#if defined(_DEBUG) && !defined(PROFILE_BUILD)
	#define SFM_NO_LOGGING 0
#else
	#define SFM_NO_LOGGING 1
#endif

// Set to 1 to let FM. BISON handle denormals
#define SFM_KILL_DENORMALS 0

// Define to disable all FX
// #define SFM_DISABLE_FX

#include "synth-log.h"
#include "synth-math.h"

namespace SFM
{
	/*
		Almost all constants used across FM. BISON are defined here; I initially chose this approach because there weren't so many
		of them and almost none were too specific to certain parts of the code. This has changed over time but I still, for now, decide
		to stick with having them here instead of spreading them across various files.
	 
		October 2019: I've now decided to *try* and place (new) constants closer to home, but only if it isn't also used by the VST!
	*/

	// Reasonable audible spectrum
	// Source: https://en.wikipedia.org/wiki/Hearing_range
	constexpr float kAudibleLowHz = 31.f;
	constexpr float kAudibleHighHz = 20000.f; 

	// Max. fixed frequency (have fun with it!)
	constexpr float kMaxFixedHz = 96000.f;

	// Default parameter cutoff (in Hz) (for ParameterFilter)
	constexpr float kDefParameterFilterCutHz = 5000.f;

	// Default parameter latency (used for per-sample interpolation of parameters and controls)
	constexpr float kDefParameterLatency = 0.016f; // 16ms (reasonable ASIO default is 16ms, approx. 60FPS)

	// Polyphony constraints
	constexpr unsigned kMinVoices  = 1;
	constexpr unsigned kMaxVoices  = 128;

	// Default number of vioces
	constexpr unsigned kDefMaxVoices = 32; // Very safe and fast
	
	// Number of FM synthesis operators (changing this value requires a thorough check)
	constexpr unsigned kNumOperators = 6;
	
	// Base note Hz (A4)
	// "Nearly all modern symphony orchestras in Germany and Austria and many in other countries in continental Europe (such as Russia, Sweden and Spain) tune to A = 443 Hz." (Wikipedia)
	constexpr double kBaseHz = 444.0; // But I don't :-)

	// Max. pitch bend range (in semitones)
	constexpr unsigned kMaxPitchBendRange = 48; // +/- 4 octaves
	constexpr unsigned kDefPitchBendRange = 12; // +/- 1 octave

	// Max. note drift (in cents, bidirectional)
	constexpr unsigned kMaxNoteJitter = 33; // Going with the number 3 again, thanks Jan Marguc :-)

	// Main filter resonance range (max. must be < 40.f, or so the manual says)
	// Engine adds kMinFilterResonance automatically!
	constexpr float kMinFilterResonance = 0.025f;
	constexpr float kMaxFilterResonance = 13.f;
	constexpr float kFilterResonanceRange = kMaxFilterResonance-kMinFilterResonance;
	
	// Min. filter cutoff; range is simply [0..1]
	constexpr float kMinFilterCutoff = 0.f;

	// Default main filter settings
	constexpr float kDefFilterCutoff    = 1.f;          // No (or minimal) filtering (when in lowpass mode, at least)
	constexpr float kDefFilterResonance = 0.f;          // Filter's default Q
	constexpr float kMinFilterCutoffHz  = 16.f;         // See impl.
	constexpr float kMainCutoffAftertouchRange = 0.66f; // Limits aftertouch cutoff to avoid that low range of the cutoff that's not allowed (SVF, < 16.0), which causes filter instability
	
	// Resonance range is limited for a smoother "knob feel" for both the main (voice) filter & the per operator filters (which will remain at this value)
	constexpr float kDefFilterResonanceLimit = 0.6f;

	// Reverb default lowpass & highpass (normalized)
	constexpr float kDefReverbFilter = 1.f;

	// Default post-pass filter settings & ranges
	// Tweak these according to filter in use (KrajeskiMoog); the idea is, roughly,
	// to be able to mix in a warm fuzzy filtered version with some extra drive on top of the sound, prior to reverberation
	constexpr float kDefPostFilterResonance =  0.f;
	constexpr float kDefPostFilterDrivedB   = -3.f;
	constexpr float kMaxPostFilterResonance =  1.f;
	constexpr float kMinPostFilterDrivedB   = -3.f;
	constexpr float kMaxPostFilterDrivedB   =  3.f;

	// Tube amp. distortion
	constexpr float kMinTubeDrivedB = 16.f;
	constexpr float kMaxTubeDrivedB = 24.f;
	constexpr float kDefTubeDrivedB = 16.f;

	// Envelope rate multiplier range (or 'global')
	// Range (as in seconds) taken from Arturia DX7-V (http://downloads.arturia.com/products/dx7-v/manual/dx7-v_Manual_1_0_EN.pdf)
	constexpr float kEnvMulMin = 0.1f;
	constexpr float kEnvMulMax = 60.f;
	constexpr float kEnvMulRange = kEnvMulMax-kEnvMulMin;

	// Multiplier on ADSR envelope ratio (release) for piano (CP) sustain pedal mode
	// The higher the value, the more linear (and thus longer) the release phase will be
	constexpr float	kEnvPianoSustainRatioMul = 1000.f;

	// Gain per voice (in dB)
	// This keeps the voice mix nicely within acceptable range (approx. 8 voices, see https://www.kvraudio.com/forum/viewtopic.php?t=275702)
	constexpr float kVoiceGaindB = -9.f;
	
	// The CP effect in synth-post-pass.cpp sounds best this way
	constexpr float kMaxCPWet = 0.707f; // -3dB

	// Reverb effect sounds best until mixed to around 50-60 percent as well (wetness)
	constexpr float kMaxReverbWet = 0.55f;

	// Chorus/Phaser speed/rate parameters (Hz)
	constexpr float kMaxChorusSpeed = 12.f;
	constexpr float kMaxPhaserSpeed = 8.f;
	
	// Master output volume range & default in dB
	constexpr int kMinVolumedB = -75;
	constexpr int kMaxVolumedB =   6;
	constexpr int kDefVolumedB =  -9;
	constexpr int kVolumeRangedB = kMaxVolumedB-kMinVolumedB;

	// (Monophonic) frequency glide (in seconds)
	constexpr float kMaxFreqGlide = 1.f;        // 1000ms
	constexpr float kDefMonoFreqGlide = 0.066f; //   66ms
	constexpr float kDefPolyFreqGlide = 0.1f;   //  100ms
	constexpr float kDefMonoGlideAtt = 0.33f;   // [0..1], the larger the punchier

	// Reverb width range & default (FIXME: odd range, no?)
	constexpr float kMinReverbWidth = 0.f;
	constexpr float kMaxReverbWidth = 2.f;
	constexpr float kDefReverbWidth = 0.5f;

	// Reverb pre-delay line max. size
	constexpr float kReverbPreDelayMax = 0.5f;   // 500ms
	constexpr float kDefReverbPreDelay = 0.001f; //  10ms

	// Compressor range & defaults
	constexpr float kDefCompPeakToRMS    =   0.f;
	constexpr float kMinCompThresholdB   = kMinVolumedB;
	constexpr float kMaxCompThresholdB   = kMaxVolumedB;
	constexpr float kDefCompThresholddB  =   0.f;
	constexpr float kMinCompKneedB       =   1.f;
	constexpr float kMaxCompKneedB       =  12.f;
	constexpr float kDefCompKneedB       =   1.f;
	constexpr float kMinCompRatio        =   1.f;
	constexpr float kMaxCompRatio        =  16.f;
	constexpr float kDefCompRatio        =   1.f; // No compression
	constexpr float kMinCompGaindB       =  -6.f;
	constexpr float kMaxCompGaindB       =  32.f; // Rather arbitrary, I want to implement automatic gain compensation (FIXME)
	constexpr float kDefCompGaindB       =   0.f;
	constexpr float kMinCompAttack       =   0.f;
	constexpr float kMaxCompAttack       =   1.f;
	constexpr float kDefCompAttack       = 0.09f; // Tested default
	constexpr float kMinCompRelease      =   0.f;
	constexpr float kMaxCompRelease      =  0.1f;
	constexpr float kDefCompRelease      = 0.03f; // Tested default

	// Auto-wah range & defaults
	constexpr float kMinWahSlack         = 0.01f;
	constexpr float kMaxWahSlack         = 0.1f;
	constexpr float kDefWahSlack         = 0.05f; // Not too harsh (lower values cause a peak ripple, something you'd have to *want* instead of have)
	constexpr float kMinWahAttack        = 0.f;
	constexpr float kMaxWahAttack        = 0.1f;
	constexpr float kDefWahAttack        = 0.025f;
	constexpr float kMinWahHold          = 0.f;
	constexpr float kMaxWahHold          = 0.1f;
	constexpr float kDefWahHold          = 0.1f;
	constexpr float kMinWahRate          = 0.f; 
	constexpr float kMaxWahRate          = 1.284220f;  // Taken from synth-DX7-LFO-table.h
	constexpr float kDefWahRate          = 0.435381f;  //

	// Low cut (for post-pass DC blocker)
	constexpr float kLowCutHz = 16.f;

	// If a voice is considered to be stolen, this bias [0..1] is multiplied with the output
	// of a voice in release state so it's preferred over an equally loud but playing voice
	constexpr float kVoiceStealReleaseBias = 0.1f;

	// Size of main delay effect's line in seconds
	constexpr float kMainDelayInSec = 4.f; // Min. 15BPM

	// Default piano pedal settings & mul. range
	constexpr float kDefPianoPedalFalloff = 0.f;    // Slowest
	constexpr float kDefPianoPedalReleaseMul = 5.f;  // A reasonable default
	constexpr float kPianoPedalMinReleaseMul = 1.f;
	constexpr float kPianoPedalMaxReleaseMul = 10.f; // Rather arbitrary, in fact I'm not sure if this should be a feature at all! (FIXME)
};

#include "synth-random.h"
#include "synth-helper.h"
// #include "synth-math.h"
// #include "synth-fast-tan.h"
// #include "synth-fast-cosine.h"
#include "synth-aligned-alloc.h"
#include "synth-ring-buffer.h"