
/*
	FM. BISON hybrid FM synthesis -- FM voice render (stereo).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Important:
		- An operator can only be modulated by an operator above it (as in index value)
		- Feedback can be taken from any level
		- This used to be a POD structure and it's not quite a C++ class (potential FIXME), initialization and manipulation of
		  members, for a large part, happens in FM_BISON.cpp and the Sample() function
*/

#pragma once

#include "3rdparty/filters/SvfLinearTrapOptimised2.hpp"
#include "3rdparty/filters/Biquad.h"

#include "synth-global.h"
#include "synth-oscillator.h"
#include "patch/synth-patch-global.h"
#include "synth-envelope.h"
#include "synth-one-pole-filters.h"
#include "synth-signal-follower.h"

namespace SFM
{
	class Voice
	{
	public:
		// Key slot (-1 means it's a rogue voice)
		int m_key;

		// Offset in samples until actually triggered (must happen within a single Render() cycle)
		unsigned m_sampleOffs;

		// Velocity
		float m_velocity;

		// Fundamental frequency
		float m_fundamentalFreq;

		enum State
		{
			kIdle      = 0, // Silent / Available
			kPlaying   = 1, // In full swing
			kReleasing = 2, // Releasing
			kStolen    = 3  // Stolen (quickly fade)
		} m_state;

		// Can be true in all non-kIdle states
		bool m_sustained;
		
		// Modulation buffer (1 sample delay, FIXME)
		float m_modSamples[kNumOperators+1]; // First slot for index -1

		struct Operator
		{
			bool enabled;
			
			// Frequency
			float setFrequency; // As calculated by CalcOpFreq()
			InterpolatedParameter<kMulInterpolate> curFreq;

			// Detune offset (used in jitter)
			float detuneOffs;

			// Key tracking (higher note, shorter envelope)
			float keyTracking;

			// Oscillator, amplitude & envelope
			InterpolatedParameter<kLinInterpolate> amplitude; // (R)
			InterpolatedParameter<kLinInterpolate> index;     // (R)
			Oscillator oscillator;
			Envelope envelope;

			// Indices: -1 means none, modulator indices must be larger than operator index
			int modulators[3], iFeedback;
			bool noModulation; // Small optimization (see Voice::Render())

			// Feedback (R)
			// See: https://www.reddit.com/r/FMsynthesis/comments/85jfrb/dx7_feedback_implementation/
			InterpolatedParameter<kLinInterpolate> feedbackAmt;
			float feedback; // Operator feedback

			// LFO influence
			float ampMod;
			float pitchMod;
			float panMod;

			// Drive (square) distortion (R)
			InterpolatedParameter<kLinInterpolate> drive;

			// Panning ([0..1], 0.5 is center) (R)
			InterpolatedParameter<kLinInterpolate> panning;

			// Is carrier (output)
			bool isCarrier;
			
			// Filters
			Biquad filter;                     // Operator filter
			SvfLinearTrapOptimised2 modFilter; // Filter can be used to take the edge off an operator to be used as modulator (Set to default by Reset(), could be a Biquad, sure, but this is tweaked to work)

			// Gain envelope
			FollowerEnvelope envGain;

			// Supersaw parameters
			InterpolatedParameter<kLinInterpolate> supersawDetune;
			InterpolatedParameter<kLinInterpolate> supersawMix;

			// This function is called by Voice::Reset()
			void Reset(unsigned sampleRate)
			{
				// Disabled
				enabled = false;
				
				// Near-zero frequency
				curFreq = { kEpsilon, sampleRate, kDefParameterLatency };

				// No detune jitter
				detuneOffs = 0.f;

				// No key tracking
				keyTracking = 0.f;

				// Silent
				amplitude = { 0.f, sampleRate, kDefParameterLatency };
				index     = { 0.f, sampleRate, kDefParameterLatency };

				// Void oscillator
				oscillator = Oscillator(sampleRate);

				// Idle envelope
				envelope.Reset();

				// No modulators
				modulators[0] = -1;
				modulators[1] = -1;
				modulators[2] = -1;
				noModulation = true;

				// No feedback input
				iFeedback = -1;

				// No feedback
				feedbackAmt = { 0.f, sampleRate, kDefParameterLatency };
				feedback = 0.f;
				
				// No modulation
				ampMod   = 0.f;
				pitchMod = 0.f;
				panMod   = 0.f;

				// No soft distortion
				drive = { 0.f, sampleRate, kDefParameterLatency };
				
				// No (manual) panning
				panning = { 0.f, sampleRate, kDefParameterLatency };

				// Not a carrier
				isCarrier = false;
				
				// Reset operator filter
				filter.reset();

				// Reset modulator filter
				modFilter.updateNone();
				modFilter.resetState();

				// Re(set) gain envelope
				envGain.Reset();
				envGain.SetSampleRate(sampleRate);
				envGain.SetAttack(12.f);   // In MS
				envGain.SetRelease(240.f); //

				// Default supersaw settings
				supersawDetune = { kDefSupersawDetune, sampleRate, kDefParameterLatency };
				supersawMix    = { kDefSupersawMix,    sampleRate, kDefParameterLatency };
			}

		} m_operators[kNumOperators];

		// LFO oscillators
		Oscillator m_LFO1, m_LFO2;
		Oscillator m_modLFO;

		// Main filter (used in FM_BISON.cpp)
		SvfLinearTrapOptimised2 m_filterSVF;
		
		// Filter (amplitude) envelope
		Envelope m_filterEnvelope;

		// Pitch (range & envelope)
		int m_pitchBendRange; // Applied to LFO, envelope & modulation
		PitchEnvelope m_pitchEnvelope;

		// Freq. glide
		float m_freqGlide;

	private:
		void ResetOperators(unsigned sampleRate);

	public:
		void Reset(unsigned sampleRate);
		
		// Call after every initialization
		void PostInitialize();

		bool IsIdle()      const { return kIdle      == m_state; }
		bool IsPlaying()   const { return kPlaying   == m_state; }
		bool IsReleasing() const { return kReleasing == m_state; }
		bool IsStolen()    const { return kStolen    == m_state; }
		bool IsSustained() const { return m_sustained;           }

		// Checks if all carrier envelopes are idle
		bool IsDone();

		// Release all envelopes & sets state to kReleasing
		void OnRelease(); 

		// Used for voice stealing & monophonic mode
		float GetSummedOutput(); /* const */

		// Render "dry" FM voice (see impl. for param. ranges)
		void Sample(float &left, float &right, float pitchBend, float ampBend /* Linear gain */, float modulation, float LFOBias, float LFOModDepth);
	};
}
