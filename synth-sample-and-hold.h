
/*
	FM. BISON hybrid FM synthesis -- Basic S&H oscillator (LFO).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	This is a simple implementation that utilizes a little bit of white noise and an oscillator (band-limited) 
	to generate a LFO S&H oscillator.

	FIXME:
	- Does not work (well) with high (read: audible) frequencies; a combination of the glide range and the high
	  frequency of both the gate signal and the signal itself renders make it sound like a broken bit crusher.
	  It is however primarily designed as LFO.
	- It would be nice to detach this class in full from the Oscillator logic; Serum for example has S&H as a
	  filter. Food for thought.

	For now this will do just fine (29/05/2020).
*/

#pragma once

#include "synth-global.h"
#include "synth-stateless-oscillators.h"
#include "synth-phase.h"
// #include "synth-one-pole-filters.h"
#include "synth-interpolated-parameter.h"

namespace SFM
{
	class SampleAndHold
	{
	public:
		SampleAndHold(unsigned sampleRate) :
			m_sampleRate(sampleRate)
,			m_curSignal(0.f, sampleRate, kDefSandHSlewRate)
		{
			m_sigPhase.Initialize(1.f, sampleRate);
		}

		~SampleAndHold() {}

		float Sample(float phase, float frequency)
		{
			const float curGate = oscSquare(phase);

			if (m_prevGate != curGate)
			{
				// Adv. phase
				m_sigPhase.Skip(m_ticks);

				// Set new (jittered) frequency
				const float freqJitter = m_freqJitter*mt_randfc()*100.f; // +/- 100 cents
				frequency *= powf(2.f, (freqJitter*0.01f)/12.f);
				m_sigPhase.SetFrequency(frequency);
				
				// Update slew rate and set signal to hold as new target
				const float curSignal = m_curSignal.Get();
				m_curSignal.SetRate(m_sampleRate, m_slewRate);
				m_curSignal.SetTarget(oscSine(m_sigPhase.Sample()));
			}
			else
				++m_ticks;

			m_prevGate = curGate;

			return m_curSignal.Sample(); // Slew towards target signal
		}

		void SetSlewRate(float rate)
		{
			SFM_ASSERT(rate >= kMinSandHSlewRate && rate <= kMaxSandHSlewRate)
			m_slewRate = rate;
		}

	private:
		// Parameters
		/* const */ unsigned m_sampleRate;

		float m_freqJitter = 1.f; // Not parametrized; default behaviour sounds fine for now
		float m_slewRate   = kDefSandHSlewRate;

		// State
		Phase m_sigPhase;
		float m_prevGate = 0.f;
		unsigned m_ticks = 0;
		InterpolatedParameter<kLinInterpolate> m_curSignal;
	};
}
