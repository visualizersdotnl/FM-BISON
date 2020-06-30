
/*
	FM. BISON hybrid FM synthesis -- Patch: FM operators.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	This is the FM operator patch: all operator settings.

	In case a parameter does not come with a comment it can be most likely be normalized ([0..1]) range,
	though in some times it helps to look at synth-global.h; it *should* be documented here so it's a FIXME.
	For details see synth-global.h & synth-patch-global.h!
*/

#pragma once

#include "../synth-global.h"

#include "../synth-oscillator.h"
#include "../synth-envelope.h"

namespace SFM
{
	// Frequency control ranges
	// IMPORTANT: coarse may *never* be 0!
	constexpr int kCoarseMin     = -4;
	constexpr int kCoarseMax     = 32;
	constexpr int kFineRange     = 24;    // Semitones
	constexpr float kDetuneRange = 100.f; // Cents

	constexpr unsigned kNumOperatorWaveforms = 7;

	const Oscillator::Waveform kOperatorWaveforms[7] =
	{
			Oscillator::Waveform::kSine,             // Sine
			Oscillator::Waveform::kPolyTriangle,     // Triangle
			Oscillator::Waveform::kPolySquare,	     // Square
			Oscillator::Waveform::kPinkNoise,        // Pink noise
			Oscillator::Waveform::kPolySaw,          // Saw
			Oscillator::Waveform::kSupersaw,         // Supersaw
			Oscillator::Waveform::kPolyRectifiedSine // Rectified sine
	};

	struct PatchOperators
	{
		// [0..1] unless stated otherwise
		// ** (R) means (to be) interpolated per-sample! **
		struct Operator
		{
			// Enabled/Carrier
			bool enabled;
			bool isCarrier;
			
			// Waveform
			Oscillator::Waveform waveform;

			// Filter parameters
			enum FilterType
			{
				kNoFilter,
				kLowpassFilter,
				kHighpassFilter,
				kBandpassFilter,
				kAllPassFilter,
				kNumFilters
			} filterType;

			float cutoff;
			float resonance;

			// Cutoff keytracking (higher key(s) means higher cutoff frequency)
			float cutoffKeyTrack;

			// Sync. oscillator on key press
			bool keySync;
			
			// Indices: -1 means none; modulators must be higher up
			unsigned modulators[3], feedback;

			// Frequency settings
			int   coarse;   // Ratio mode: integer ratio (-1 = /2, 2 = *2 et cetera, 0 is interpreted as 1) 
			                // Fixed mode: any positive integer frequency
			int     fine;   // Semitones
			float detune;   // Cents

			// Fixed frequency (use coarse value only)
			bool fixed;

			// Output level (normalized) (R)
			float output;

			// Envelopes
			Envelope::Parameters envParams;
			float envKeyTrack;        // Tracks the envelope along the keys (higher keys shorter)
			bool acousticEnvKeyTrack; // An acoustic piano type curve is optional

			// Velocity invert (play "louder" for less amplitude/index)
			bool velocityInvert;

			// Velocity sensitivity
			float velSens;

			// Feedback amount (R)
			float feedbackAmt;

			// LFO influence (on the DX7 this is global)
			float ampMod;
			float pitchMod;
			float panMod;

			// Drive (square) distortion (R)
			float drive;

			// Panning [-1..1] (R)
			float panning;

			// Level scaling
			unsigned levelScaleBP;               // [0..127]
			unsigned levelScaleRange;            // In number of semitones
			float levelScaleL, levelScaleR;      // [-1..1], where [0..-1] is subtractive & [0..1] is additive
			bool levelScaleExpL, levelScaleExpR; // Linear or exponential

			// Break point cut: if enabled will cut all output left or right of the break point (levelScaleBP)
			// If both are set 'levelScaleBP' means a range of notes starting at the highest C relative to it
			bool cutLeftOfLSBP;
			bool cutRightOfLSBP;

			// Parameters for this operator's supersaw oscillator (just to be sure: both are [0..1])
			float supersawDetune;
			float supersawMix;
		};
	
		Operator operators[kNumOperators];

		void ResetToEngineDefaults()
		{
			for (auto &patchOp : operators)
			{	
				patchOp.enabled = false;
				patchOp.isCarrier = false;
				
				// Sine
				patchOp.waveform = kOperatorWaveforms[0];
				
				// No filter
				patchOp.filterType = Operator::kNoFilter;
				patchOp.cutoff = kDefMainFilterCutoff;
				patchOp.resonance = kDefMainFilterResonance;
				patchOp.cutoffKeyTrack = 0.f; // No filter key tracking (or 'TRACK', if you will)

				// Sync. oscillator by default
				patchOp.keySync = true;

				// No modulators
				patchOp.modulators[0] = patchOp.modulators[1] = patchOp.modulators[2] = unsigned(-1);

				// Neutral tone
				patchOp.coarse = 1;
				patchOp.fine   = 0;
				patchOp.detune = 0.f;

				// Ratio modes
				patchOp.fixed = false;

				// Output level
				patchOp.output = 1.f;

				// Envelope: full sustain only, linear curvature
				patchOp.envParams.preAttack = 0.f;
				patchOp.envParams.attack = 0.f;
				patchOp.envParams.decay = 0.f;
				patchOp.envParams.sustain = 1.f;
				patchOp.envParams.release = 0.f;
				patchOp.envParams.attackCurve = 0.f;
				patchOp.envParams.decayCurve = 0.f;
				patchOp.envParams.releaseCurve = 0.f;
				patchOp.envParams.rateMul = 1.f; // 1 second
				
				// No key tracking
				patchOp.envKeyTrack = 0.f;
				patchOp.acousticEnvKeyTrack = false;

				// No velocity invert
				patchOp.velocityInvert = false;

				// Not velocity sensitive
				patchOp.velSens = 0.f;

				// No feedback
				patchOp.feedback = unsigned(-1);
				patchOp.feedbackAmt = 0.f;

				// No LFO influence
				patchOp.ampMod = 0.f;
				patchOp.pitchMod = 0.f;
				patchOp.panMod = 0.f;

				// No panning
				patchOp.panning = 0.f;

				// No drive
				patchOp.drive = 0.f;

				// No level scaling (though defined around Mid. C)
				patchOp.levelScaleBP = 60;    // Mid. C
				patchOp.levelScaleRange = 0;  // Disabled
				patchOp.levelScaleL = 0.f;
				patchOp.levelScaleR = 0.f;
				patchOp.levelScaleExpL = false;
				patchOp.levelScaleExpR = false;

				// No cuts
				patchOp.cutLeftOfLSBP  = false;
				patchOp.cutRightOfLSBP = false;

				// Default supersaw
				patchOp.supersawDetune = 0.f;
				patchOp.supersawMix = 0.f;
			}
		}
	};
}
