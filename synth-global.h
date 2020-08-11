
/*
	FM. BISON hybrid FM synthesis -- Global includes, constants & utility functions: include on top of every header or autonomous CPP.
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
#define SFM_KILL_DENORMALS 1

// Define to disable all FX (including per-voice filter)
// #define SFM_DISABLE_FX

// Define to disable extra voice rendering thread
// #define SFM_DISABLE_VOICE_THREAD

namespace SFM
{
	/*
		Almost all contants used by FM. BISON and it's host are defined here; I initially chose to stick them in one place because 
		this was a much smaller project and there simply weren't too many them

		This changed over time and now more and more of them are defined close to or in the implementation they're relevant instead; 
		new ones that go here should be either used in different places or, important: be of use by the host software

		I added some comment blocks to make this easier to navigate
	*/

	// ----------------------------------------------------------------------------------------------
	// Voices
	// ----------------------------------------------------------------------------------------------

	// Max. number of voices to render using the main (single) thread
	// 32 is based on 64 being a reasonable total
	// FIXME: make this or rather using the second thread at all a setting?
	constexpr unsigned kSingleThreadMaxVoices = 32;

	// Max. fixed frequency (have fun with it!)
	constexpr float kMaxFixedHz = 96000.f;

	// Polyphony constraints
	constexpr unsigned kMinVoices = 1;
	constexpr unsigned kMaxVoices = 128;

	// Default number of vioces
	constexpr unsigned kDefMaxVoices = 32; // Safe and fast

	// ----------------------------------------------------------------------------------------------
	// Parameter latency & slew
	// ----------------------------------------------------------------------------------------------

	// Default parameter latency (used for per-sample interpolation of parameters and controls)
	constexpr float kDefParameterLatency = 0.01f; // 10MS

	// Default ParameterSlew MS (sampled per frame (Render() call))
	constexpr float kDefParameterSlewMS = 5.f;
	
	// ----------------------------------------------------------------------------------------------
	// Number of FM synthesis operators (changing this value requires a thorough check)
	// ----------------------------------------------------------------------------------------------

	constexpr unsigned kNumOperators = 6;

	// ----------------------------------------------------------------------------------------------
	// Base note Hz (A4)
	//
	// "Nearly all modern symphony orchestras in Germany and Austria and many in other countries in 
	//  continental Europe (such as Russia, Sweden and Spain) tune to A = 443 Hz." (Wikipedia)
	// ----------------------------------------------------------------------------------------------

	constexpr float kBaseHz = 440.f;

	// ----------------------------------------------------------------------------------------------
	// Pitch bend range
	// ----------------------------------------------------------------------------------------------

	// Max. pitch bend range (in semitones)
	constexpr unsigned kMaxPitchBendRange = 48; // +/- 4 octaves
	constexpr unsigned kDefPitchBendRange = 12; // +/- 1 octave

	// ----------------------------------------------------------------------------------------------
	// Jitter
	// ----------------------------------------------------------------------------------------------

	// Jitter: max. note drift (in cents, -/+)
	constexpr unsigned kMaxNoteJitter = 50; // Half a note

	// Jitter: max. detune (in cents, -/+)
	constexpr float kMaxDetuneJitter = 1.f; // 100th of a note

	// ----------------------------------------------------------------------------------------------
	// Tuning range (no more no less; just to derive Hz and dB from)
	// ----------------------------------------------------------------------------------------------

	constexpr float kMinTuning = -12.f;
	constexpr float kMaxTuning =  12.f;

	// ----------------------------------------------------------------------------------------------
	// Filter
	// ----------------------------------------------------------------------------------------------

	// Per operator filter peak dB range
	constexpr float kMinOpFilterPeakdB =  -24.f;
	constexpr float kMaxOpFilterPeakdB =   12.f;
	constexpr float kDefOpFilterPeakdB =   -3.f;

	// Main (SVF) filter Q range (not to be confused with normalized resonance, max. must be <= 40.f, the impl. says)
	// Helper function ResoToQ() scales to range automatically
	constexpr float kSVFLowestFilterQ = 0.025f; // Use carefully, a Q has shown to cause instability (filter slowly 'blowing up')
	constexpr float kSVFMinFilterQ    = 0.5f;   // See https://www.earlevel.com/main/2003/03/02/the-digital-state-variable-filter/
	constexpr float kSVFMaxFilterQ    = 40.f;   // Actual max. is 40.f
	constexpr float kSVFFilterQRange  = kSVFMaxFilterQ-kSVFMinFilterQ;

	constexpr float kMinFilterCutoff      =  0.f; // Normalized min. filter cutoff; range is simply [0..1] (use SVF_CutoffToHz())
	constexpr float kSVFMinFilterCutoffHz = 20.f; // Min 16.f (See impl.)

	// Default main (SVF) filter settings
	constexpr float kDefMainFilterCutoff       =   1.f; // Normalized; no (or minimal) filtering (when acting as LPF at least)
	constexpr float kMainCutoffAftertouchRange = 0.66f; // Limits aftertouch cutoff to avoid that low range of the cutoff that's not allowed (SVF, < 16.0), which may cause filter instability
	constexpr float kDefMainFilterResonance    =   0.f; // Filter's default normalized resonance
	
	// Normalized resonance range can be limited of the main voice filter
	constexpr float kDefMainFilterResonanceLimit = 0.33f;

	// Reverb default lowpass & highpass (normalized)
	constexpr float kDefReverbFilter = 1.f;

	// Default post-pass filter drive range & default (dB)	
	constexpr float kMinPostFilterDrivedB = -3.f;
	constexpr float kMaxPostFilterDrivedB =  9.f;
	constexpr float kDefPostFilterDrivedB =  3.f;

	// Post-pass filter cutoff range
	constexpr float kMinPostFilterCutoffHz = 40.f;
	constexpr float kMaxPostFilterCutoffHz = 20000.f;
	constexpr float kPostFilterCutoffRange = kMaxPostFilterCutoffHz-kMinPostFilterCutoffHz;

	// Usual magnitude (gain) response at cutoff point (it's 1.0/sqrt(2.0))
	constexpr float kDefGainAtCutoff = 0.707106769f;

	// ----------------------------------------------------------------------------------------------
	// Tube distortion
	// ----------------------------------------------------------------------------------------------

	// Tube distortion drive & offset
	constexpr float kMinTubeDrive  =   1.f;
	constexpr float kMaxTubeDrive  = 100.f;
	constexpr float kDefTubeDrive  =  10.f;
	constexpr float kMinTubeOffset = -0.1f;
	constexpr float kMaxTubeOffset =  0.1f;

	// ----------------------------------------------------------------------------------------------
	// Anti-aliasing (basically just a modest LPF) (during oversampled stage)
	// ----------------------------------------------------------------------------------------------

	constexpr float kDefAntiAliasing = 0.f;

	// ----------------------------------------------------------------------------------------------
	// Envelope
	// ----------------------------------------------------------------------------------------------

	// Envelope rate multiplier range (or 'global')
	// Range (as in seconds) taken from Arturia DX7-V 
	// Ref. http://downloads.arturia.com/products/dx7-v/manual/dx7-v_Manual_1_0_EN.pdf
	constexpr float kEnvMulMin = 0.1f;
	constexpr float kEnvMulMax = 60.f;
	constexpr float kEnvMulRange = kEnvMulMax-kEnvMulMin;

	// Multiplier on ADSR envelope ratio (release) for piano (CP) sustain pedal mode
	// The higher the value, the more linear (and thus longer) the release phase will be
	constexpr float	kEnvPianoSustainRatioMul = 1000.f;

	// ----------------------------------------------------------------------------------------------
	// Gain per voice (in dB)
	// This keeps the voice mix nicely within acceptable range, approx. 8 voices, 
	// see https://www.kvraudio.com/forum/viewtopic.php?t=275702
	// ----------------------------------------------------------------------------------------------

	constexpr float kVoiceGaindB = -9.f;

	// ----------------------------------------------------------------------------------------------
	// Chorus/Phaser
	// ----------------------------------------------------------------------------------------------
	
	// Chorus/Phaser effect (synth-post-pass.cpp) max. wetness
	constexpr float kMaxChorusPhaserWet = 0.707f; // -3dB

	// Chorus/Phaser rate multipliers (Hz)
	constexpr float kMaxChorusRate = 12.f;
	constexpr float kMaxPhaserRate = 8.f;

	// ----------------------------------------------------------------------------------------------
	// Reverb effect sounds best until mixed to max. 50-60 percent (wetness)
	// ----------------------------------------------------------------------------------------------

	constexpr float kMaxReverbWet = 0.55f;

	// ----------------------------------------------------------------------------------------------
	// Master output volume range & default in dB + our definition of -INF
	// ----------------------------------------------------------------------------------------------

	constexpr int kMinVolumedB   =   -96;
	constexpr int kMaxVolumedB   =     3;
	constexpr int kDefVolumedB   =   -12;
	constexpr int kVolumeRangedB = kMaxVolumedB-kMinVolumedB;

	// Nicked from juce::Decibels
	constexpr int   kInfdB  = -100; 
	constexpr float kInfLin = 9.99999975e-06f; // dB2Lin(kInfdB)

	// ----------------------------------------------------------------------------------------------
	// (Monophonic) frequency glide (in seconds)
	// ----------------------------------------------------------------------------------------------

	constexpr float kMaxFreqGlide = 1.f;        // 1000MS
	constexpr float kDefMonoFreqGlide = 0.066f; //   66MS
	constexpr float kDefPolyFreqGlide = 0.1f;   //  100MS
	constexpr float kDefMonoGlideAtt = 0.33f;   // [0..1], the larger the punchier

	// ----------------------------------------------------------------------------------------------
	// Slew parameters for S&H
	// ----------------------------------------------------------------------------------------------

	constexpr float kMinSandHSlewRate = 0.001f;  //  1MS
	constexpr float kMaxSandHSlewRate =  0.05f;  // 50MS
	constexpr float kDefSandHSlewRate = 0.005f;  //  5MS

	// ----------------------------------------------------------------------------------------------
	// Reverb
	// ----------------------------------------------------------------------------------------------

	// Reverb width range & default
	constexpr float kMinReverbWidth = 0.f;
	constexpr float kMaxReverbWidth = 2.f;
	constexpr float kDefReverbWidth = 0.5f;

	// ----------------------------------------------------------------------------------------------
	// Compressor range & defaults
	// ----------------------------------------------------------------------------------------------

	constexpr float kMinCompThresholdB  =  -60.f; 
	constexpr float kMaxCompThresholdB  =    6.f;
	constexpr float kDefCompThresholddB = kMaxCompThresholdB;
	constexpr float kMinCompKneedB      =    0.f;
	constexpr float kMaxCompKneedB      =   12.f;
	constexpr float kDefCompKneedB      = kMinCompKneedB;
	constexpr float kMinCompRatio       =    1.f;
	constexpr float kMaxCompRatio       =   20.f;
	constexpr float kDefCompRatio       = kMinCompRatio;
	constexpr float kMinCompGaindB      =   -6.f;
	constexpr float kMaxCompGaindB      =   60.f;
	constexpr float kDefCompGaindB      =    0.f;
	constexpr float kMinCompAttack      = 0.001f; // 1MS
	constexpr float kMaxCompAttack      =    1.f; // 1 sec.
	constexpr float kDefCompAttack      = 0.800f; // 800MS
	constexpr float kMinCompRelease     = 0.001f; // 1MS
	constexpr float kMaxCompRelease     =    1.f; // 1 sec.
	constexpr float kDefCompRelease     = 0.700f; // 100MS

	// ----------------------------------------------------------------------------------------------
	// Auto-wah range & defaults
	// ----------------------------------------------------------------------------------------------

	constexpr float kDefWahResonance     =   0.5f; // 50%
	constexpr float kMinWahAttack        = 0.001f; // 1MS
	constexpr float kMaxWahAttack        =    1.f; // 1 sec.
	constexpr float kDefWahAttack        = 0.350f; // 350MS 
	constexpr float kMinWahHold          = 0.001f; // 1MS
	constexpr float kMaxWahHold          =    1.f; // 1 sec.
	constexpr float kDefWahHold          =   0.4f; // 400MS

	constexpr float kMinWahRate          =    0.f; // DX7 rate (synth-DX7-LFO-table.h)
	constexpr float kMaxWahRate          =  100.f; // 
	constexpr float kDefWahRate          =    8.f; //

	constexpr float kMinWahDrivedB       =  -12.f; 
	constexpr float kMaxWahDrivedB       =   12.f;
	constexpr float kDefWahDrivedB       =    3.f;

	constexpr float kMaxWahSpeakVowel    =    3.f;

	// ----------------------------------------------------------------------------------------------
	// Size of main delay effect's line in seconds & drive (dB) range
	// ----------------------------------------------------------------------------------------------

	constexpr float kMainDelayInSec = 8.f; // Min. 7.5BPM

	// Defined here because kMainDelayInSec dictates it

	// FIXME: use to check if rate is in bounds in FM_BISON.cpp!
	constexpr float kMinBPM = 60.f/kMainDelayInSec;

	constexpr float kMinDelayDrivedB = -12.f;
	constexpr float kMaxDelayDrivedB =  12.f;
	constexpr float kDefDelayDrivedB =   0.f;

	// ----------------------------------------------------------------------------------------------
	// Default piano pedal settings & mul. range
	// ----------------------------------------------------------------------------------------------

	constexpr float kDefPianoPedalFalloff = 0.f; // Slowest
	constexpr float kPianoPedalMinReleaseMul = 1.f;
	constexpr float kPianoPedalMaxReleaseMul = 10.f;                     // Rather arbitrary, in fact I'm not sure if this should be a feature at all! (FIXME)
	constexpr float kDefPianoPedalReleaseMul = kPianoPedalMinReleaseMul; // So because of that, by default, the influence of this parameter is nil.
	
	// ----------------------------------------------------------------------------------------------
	// Modulator input is low passed a little bit for certain waveforms to "take the top off", 
	// so modulation sounds less glitchy out of the box
	// ----------------------------------------------------------------------------------------------

	constexpr float kModulatorLP = 0.875f; // Normalized range [0..1]

	// ----------------------------------------------------------------------------------------------
	// LFO modulation speed in steps (exponential)
	// ----------------------------------------------------------------------------------------------

	constexpr int kMinLFOModSpeed = -8;
	constexpr int kMaxLFOModSpeed =  8;

	// ----------------------------------------------------------------------------------------------
	// Default supersaw (JP-8000) parameters
	// ----------------------------------------------------------------------------------------------

	constexpr float kDefSupersawDetune = 0.f; // Zodat je *direct* door hebt dat je hem moet stellen!
	constexpr float kDefSupersawMix    = 0.f; //

	// ----------------------------------------------------------------------------------------------
	// LowBlocker freq.
	// ----------------------------------------------------------------------------------------------

	constexpr float kLowBlockerHz = 40.f;
};

// All helper functionality is at your disposal by default
// I might *not* want to do this if the set grows significantly bigger
#include "helper/synth-log.h"
#include "helper/synth-math.h"
#include "helper/synth-random.h"
#include "helper/synth-helper.h"
#include "helper/synth-fast-tan.h"
#include "helper/synth-fast-cosine.h"
#include "helper/synth-aligned-alloc.h"
#include "helper/synth-ring-buffer.h"
#include "helper/synth-MIDI.h"
