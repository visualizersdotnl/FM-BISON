
/*
	FM. BISON hybrid FM synthesis -- FM voice render (stereo).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Important:
		- An operator can only be modulated by an operator above it (as in index value)
		- Feedback can be taken from any level
		- Interface is incomplete and most voice management is implemented in FM_BISON.cpp,
		  this class needs to be initialized manually (potential FIXME)
*/

#pragma once


#include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"
#include "synth-oscillator.h"
#include "synth-patch-global.h"
#include "synth-envelope.h"
#include "synth-interpolated-parameter.h"
#include "synth-one-pole-filters.h"
// #include "synth-pitch-envelope.h"
#include "synth-MIDI.h"

namespace SFM
{
	// Number of cascaded filters of this type (keep it civil as it's a per operator filter)
	constexpr unsigned kNumVoiceAllpasses = 4;

	class Voice
	{
	public:
		// Key slot (-1 means it's a rogue key)
		int m_key;

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
			Oscillator oscillator;
			Envelope envelope;

			// Indices: -1 means none, modulator indices must be larger than operator index
			int modulators[3], iFeedback;

			// Feedback (R)
			// See: https://www.reddit.com/r/FMsynthesis/comments/85jfrb/dx7_feedback_implementation/
			InterpolatedParameter<kLinInterpolate> feedbackAmt;

			// LFO influence
			float ampMod;
			float pitchMod;
			float panMod;

			// Drive (square) distortion (R)
			InterpolatedParameter<kLinInterpolate> drive;

			// Panning (angle) (R)
			InterpolatedParameter<kLinInterpolate> panAngle;

			// Is carrier (output)
			bool isCarrier;
			
			// 12dB filter(s)
			SvfLinearTrapOptimised2 filterSVF[kNumVoiceAllpasses];

			// Signal feedback
			float feedback;

			void Reset(unsigned sampleRate)
			{
				enabled = false;
				
				curFreq = { kEpsilon, sampleRate, kDefParameterLatency };

				detuneOffs = 0.f;

				keyTracking = 0.f;

				amplitude  = { 0.f, sampleRate, kDefParameterLatency };
				oscillator = Oscillator(sampleRate);
				envelope.Reset();

				modulators[0] = -1;
				modulators[1] = -1;
				modulators[2] = -1;

				iFeedback   = -1;
				feedbackAmt = { 0.f, sampleRate, kDefParameterLatency };

				ampMod   = 0.f;
				pitchMod = 0.f;
				panMod   = 0.f;

				drive = { 0.f, sampleRate, kDefParameterLatency };
				
				panAngle = { 0.f, sampleRate, kDefParameterLatency };

				isCarrier = false;
				
				for (auto &filter : filterSVF)
					filter.resetState();

				feedback = 0.f;
			}

		} m_operators[kNumOperators];

		// LFO
		Oscillator m_LFO1, m_LFO2;
		Oscillator m_blendLFO;

		// Main filter(s)
		SvfLinearTrapOptimised2 m_filterSVF1, m_filterSVF2;
		
		// Filter (amplitude) envelope
		Envelope m_filterEnvelope;

		// Pitch envelope
		PitchEnvelope m_pitchEnvelope;

		// Freq. glide
		float freqGlide;

	private:
		void ResetOperators(unsigned sampleRate);

	public:
		void Reset(unsigned sampleRate);

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

		// Render "dry" FM voice
		void Sample(float &left, float &right, float pitchBend, float ampBend, float modulation, float LFOBias);
	};
}
