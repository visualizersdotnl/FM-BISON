
/*
	FM. BISON hybrid FM synthesis -- S&H oscillator (or rather a filter).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
	- Tighter integration with Oscillator
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
		}

		~SampleAndHold() {}

		float Sample(float phase, float input)
		{
			const float curGate = oscSquare(phase);

			if (m_prevGate != curGate)
			{
				// Update slew rate and set signal to hold as new target
				const float curSignal = m_curSignal.Get();
				m_curSignal.SetRate(m_sampleRate, m_slewRate);
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

	private:
		// Parameters
		/* const */ unsigned m_sampleRate;

		float m_slewRate = kDefSandHSlewRate;

		// State
		float m_prevGate = 0.f;
		InterpolatedParameter<kLinInterpolate> m_curSignal;
	};
}
