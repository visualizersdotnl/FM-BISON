
/*
	FM. BISON hybrid FM synthesis -- Oscillator (VCO/LFO).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- I'm not happy about Oscillator containing specific state and multiple phase objects just to
		  support a handful of special cases
		- https://github.com/bipolaraudio/FM-BISON/issues/84
*/

#pragma once

#pragma warning(push)
#pragma warning(disable: 4324) // Tell MSVC to shut it about padding I'm aware of

#include "synth-global.h"
#include "synth-phase.h"
#include "synth-stateless-oscillators.h"
#include "synth-pink-noise.h"
#include "synth-sample-and-hold.h"
#include "synth-supersaw.h"

namespace SFM
{
	class Oscillator
	{
	public:
		// Supported waveforms
		enum Waveform
		{
			kNone,

			// Band-limited
			kSine,
			kCosine,
			kPolyTriangle,
			kPolySquare,
			kPolySaw,
			kPolyRamp,
			kPolyRectifiedSine,
			kPolyRectangle,
			kBump,

			// Very soft approximation of ramp & saw (for LFO)
			kSoftRamp,
			kSoftSaw,
			
			// Supersaw
			kSupersaw,

			// Uni-polar ramp (or "fake" hard sync. & PWM)
			kCutRamp, 

			// Raw/LFO
			kRamp,
			kSaw,
			kSquare,
			kTriangle,
			kPulse,

			// Noise
			kWhiteNoise,
			kPinkNoise,

			// S&H (for LFO)
			kSampleAndHold
		};

	private:
		/* const */ Waveform m_form;
		Phase m_phase;

		// Autonomous oscillators
		PinkNoise     m_pinkNoise;
		SampleAndHold m_sampleAndHold;
		Supersaw      m_supersaw;

		// Signal
		float m_signal = 0.f;

	public:
		Oscillator(unsigned sampleRate = 1) :
			m_sampleAndHold(sampleRate)
		{
			Initialize(kNone, 0.f, sampleRate, 0.0);
		}

		void Initialize(Waveform form, float frequency, unsigned sampleRate, float phaseShift, float supersawDetune = 0.f, float supersawMix = 0.f);

		SFM_INLINE void PitchBend(float bend)
		{
			if (kSupersaw != m_form)
				m_phase.PitchBend(bend);
			else
				m_supersaw.PitchBend(bend);
		}

		SFM_INLINE void SetFrequency(float frequency)
		{
			if (kSupersaw != m_form)
				m_phase.SetFrequency(frequency);
			else
				m_supersaw.SetFrequency(frequency);
		}
		
		SFM_INLINE void Reset()
		{
			SFM_ASSERT(kSupersaw != m_form && kPinkNoise != m_form && kWhiteNoise != m_form);
			m_phase.Reset();
		}

		SFM_INLINE float GetFrequency() const 
		{ 
			return (m_form != kSupersaw)
				? m_phase.GetFrequency()
				: m_supersaw.GetFrequency();
		}

		// Useless for kPinkNoise & kWhiteNoise (but allowed)
		SFM_INLINE float GetPhase() const 
		{
			return (m_form != kSupersaw)
				? m_phase.Get()
				: m_supersaw.GetPhase();
		} 
		
		// S&H
		SFM_INLINE void SetSampleAndHoldSlewRate(float rate)
		{
			m_sampleAndHold.SetSlewRate(rate);
		}

		// Supersaw
		SFM_INLINE Supersaw &GetSupersaw()
		{
			return m_supersaw;
		}

		float Sample(float phaseShift);
	};
}

#pragma warning(pop)
