
/*
	FM. BISON hybrid FM synthesis -- S&H oscillator (or rather a filter).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"
#include "synth-stateless-oscillators.h"
#include "synth-phase.h"
#include "synth-interpolated-parameter.h"
#include "synth-MIDI.h"

namespace SFM
{
	class SampleAndHold
	{
	public:
		SampleAndHold(unsigned sampleRate) :
			m_sampleRate(sampleRate)
,			m_curSignal(0.f, sampleRate, kDefSandHSlewRate)
		{
		}

		~SampleAndHold() {}

		// Takes in any continuous signal
		float Sample(float phase, float input)
		{
			const float curGate = oscPulse(phase, m_dutyCycle);

			if (1.f == curGate && m_prevGate != curGate)
			{
				// Update slew rate and set signal to hold as new target
				const float curSignal = m_curSignal.Get();
				m_curSignal.SetRate(m_sampleRate, m_slewRate);
				m_curSignal.Set(curSignal);
				m_curSignal.SetTarget(input);
			}

			m_prevGate = curGate;

			return m_curSignal.Sample(); // Slew towards target signal
		}

		void SetSlewRate(float rate)
		{
			SFM_ASSERT(rate >= kMinSandHSlewRate && rate <= kMaxSandHSlewRate)
			m_slewRate = rate;
		}

		void SetDutyCycle(float length)
		{
			SFM_ASSERT(length >= kMinSandHDutyCycle && length <= kMaxSandHDutyCycle);
			m_dutyCycle = length;
		}

		void Reset()
		{
			m_prevGate = -1.f;
			m_curSignal.Set(0.f);
		}

	private:
		// Parameters
		/* const */ unsigned m_sampleRate;

		float m_slewRate  = kDefSandHSlewRate;
		float m_dutyCycle = kDefSandHDutyCycle;

		// State
		float m_prevGate = 0.f;
		InterpolatedParameter<kLinInterpolate> m_curSignal;
	};
}
