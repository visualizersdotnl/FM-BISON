
/*
	FM. BISON hybrid FM synthesis -- Global includes, constants & utility functions.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

// Include JUCE
// #include "../JuceLibraryCode/JuceHeader.h"

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
	const float kAudibleLowHz = 31.f;
	const float kAudibleHighHz = 20000.f; 

	// Max. fixed frequency (have fun with it!)
	const float kMaxFixedHz = 96000.f;

	// Default parameter cutoff (in Hz) (for ParameterFilter)
	const float kDefParameterFilterCutHz = 5000.f;

	// Default parameter latency (used for per-sample interpolation of parameters and controls)
	const float kDefParameterLatency = 0.016f; // 16ms (reasonable ASIO default is 16ms, approx. 60FPS)

	// Polyphony constraints
	const unsigned kMinVoices  = 1;
	const unsigned kMaxVoices  = 128;

	// Default number of vioces
	const unsigned kDefMaxVoices = 32; // Very safe and fast
	
	// Number of FM synthesis operators (changing this value requires a thorough check)
	const unsigned kNumOperators = 6;
	
	// Base note Hz (A4)
	// "Nearly all modern symphony orchestras in Germany and Austria and many in other countries in continental Europe (such as Russia, Sweden and Spain) tune to A = 443 Hz." (Wikipedia)
	const double kBaseHz = 444.0; // But I don't :-)

	// Max. pitch bend range (in semitones)
	const unsigned kMaxPitchBendRange = 48; // +/- 4 octaves
	const unsigned kDefPitchBendRange = 12; // +/- 1 octave

	// Max. note drift (in cents, bidirectional)
	const unsigned kMaxNoteJitter = 33; // Going with the number 3 again, thanks Jan Marguc :-)

	// Main filter resonance range (max. must be < 40.f, or so the manual says)
	// Engine adds kMinFilterResonance automatically!
	const float kMinFilterResonance = 0.025f;
	const float kMaxFilterResonance = 13.f;
	const float kFilterResonanceRange = kMaxFilterResonance-kMinFilterResonance;
	
	// Min. filter cutoff; range is simply [0..1]
	const float kMinFilterCutoff = 0.f;

	// Default main filter settings
	const float kDefFilterCutoff    = 1.f;          // No (or minimal) filtering (when in lowpass mode, at least)
	const float kDefFilterResonance = 0.f;          // Filter's default Q
	const float kMinFilterCutoffHz  = 16.f;         // See impl.
	const float kMainCutoffAftertouchRange = 0.66f; // Limits aftertouch cutoff to avoid that low range of the cutoff that's not allowed (SVF, < 16.0), which causes filter instability
	
	// Resonance range is limited for a smoother "knob feel" for both the main (voice) filter & the per operator filters (which will remain at this value)
	const float kDefFilterResonanceLimit = 0.6f;

	// Reverb default lowpass & highpass (normalized)
	const float kDefReverbFilter = 1.f;

	// Default post-pass filter settings & ranges
	// Tweak these according to filter in use (KrajeskiMoog); the idea is, roughly,
	// to be able to mix in a warm fuzzy filtered version with some extra drive on top of the sound, prior to reverberation
	const float kDefPostFilterResonance =  0.f;
	const float kDefPostFilterDrivedB   = -3.f;
	const float kMaxPostFilterResonance =  1.f;
	const float kMinPostFilterDrivedB   = -3.f;
	const float kMaxPostFilterDrivedB   =  3.f;

	// Tube amp. distortion
	const float kMinTubeDrivedB = 16.f;
	const float kMaxTubeDrivedB = 24.f;
	const float kDefTubeDrivedB = 16.f;

	// Envelope rate multiplier range (or 'global')
	// Range (as in seconds) taken from Arturia DX7-V (http://downloads.arturia.com/products/dx7-v/manual/dx7-v_Manual_1_0_EN.pdf)
	const float kEnvMulMin = 0.1f;
	const float kEnvMulMax = 60.f;
	const float kEnvMulRange = kEnvMulMax-kEnvMulMin;

	// Multiplier on ADSR envelope ratio (release) for piano (CP) sustain pedal mode
	// The higher the value, the more linear (and thus longer) the release phase will be
	const float	kEnvPianoSustainRatioMul = 1000.f;

	// Gain per voice (in dB)
	// This keeps the voice mix nicely within acceptable range (approx. 8 voices, see https://www.kvraudio.com/forum/viewtopic.php?t=275702)
	const float kVoiceGaindB = -9.f;
	
	// The CP effect in synth-post-pass.cpp sounds best this way
	const float kMaxCPWetdB = -3.f;

	// Reverb effect sounds best until mixed to around 50-60 percent as well (wetness)
	const float kMaxReverbWet = 0.55f;

	// Chorus/Phaser speed/rate parameters (Hz)
	const float kMaxChorusSpeed = 12.f;
	const float kMaxPhaserSpeed = 8.f;
	
	// Master output volume range & default in dB
	const int kMinVolumedB = -75;
	const int kMaxVolumedB =   6;
	const int kDefVolumedB =  -9;
	const int kVolumeRangedB = kMaxVolumedB-kMinVolumedB;

	// (Monophonic) frequency glide (in seconds)
	const float kMaxFreqGlide = 1.f;        // 1000ms
	const float kDefMonoFreqGlide = 0.066f; //   66ms
	const float kDefPolyFreqGlide = 0.1f;   //  100ms
	const float kDefMonoGlideAtt = 0.33f;   // [0..1], the larger the punchier

	// Reverb width range & default (FIXME: odd range, no?)
	const float kMinReverbWidth = 0.f;
	const float kMaxReverbWidth = 2.f;
	const float kDefReverbWidth = 0.5f;

	// Reverb pre-delay line max. size
	const float kReverbPreDelayMax = 0.5f;   // 500ms
	const float kDefReverbPreDelay = 0.001f; //  10ms

	// Compressor range & defaults
	const float kMinCompThresholdB   = kMinVolumedB;
	const float kMaxCompThresholdB   =   1.f;
	const float kDefCompThresholddB  =  -6.f;
	const float kMinCompKneedB       =   1.f;
	const float kMaxCompKneedB       = -24.f;
	const float kDefCompKneedB       =   3.f;
	const float kMinCompLookaheadMS  =   0.f;
	const float kMaxCompLookaheadMS  =  10.f;
	const float kDefCompLookaheadMS  =   1.f;
	const float kMinCompRatio        =   1.f;
	const float kMaxCompRatio        =  20.f;
	const float kDefCompRatio        =   1.f;
	const float kMinCompAttack       =   0.f;
	const float kMaxCompAttack       =  1.0f;
	const float kDefCompAttack       =  0.1f; // Sec.
	const float kMinCompRelease      =   0.f;
	const float kMaxCompRelease      =   1.f;
	const float kDefCompRelease      =  0.3f; // Sec.
	 
	// Low cut (for post-pass DC blocker)
	const float kLowCutHz = 40.f;

	// If a voice is considered to be stolen, this bias [0..1] can pull it down if the voice is
	// in a release state, which means it's chance of being stolen is higher
	const float kVoiceStealReleaseBias = 0.66f;

	// Size of main delay effect's line in seconds
	const float kMainDelayInSec = 4.f; // Min. 15BPM

	// Default piano pedal settings & mul. range
	const float kDefPianoPedalFalloff = 0.f;    // Slowest
	const float kDefPianoPedalReleaseMul = 5.f; // A reasonable default
	const float kPianoPedalMinReleaseMul = 1.f;
	const float kPianoPedalMaxReleaseMul = 100.f; // FIXME: why *this* much?
};

#include "synth-random.h"
#include "synth-helper.h"
#include "synth-aligned-alloc.h"
