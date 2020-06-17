
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

		SFM_INLINE float Sample(float phase, float input)
		{
			const float curGate = oscBox(phase);
			
			// Gate?
			if (m_prevGate != curGate)
			{
				// Update slew rate and set signal to hold as new target
				const float curSignal = m_curSignal.Sample();
				m_curSignal.SetRate(m_sampleRate, m_slewRate);
				m_curSignal.Set(curSignal);
				m_curSignal.SetTarget(input);
			}

			m_prevGate = curGate;

			// Slew towards target signal 
			return Clamp(m_curSignal.Sample()); // Clamp to gaurantee output range
		}

		void SetSlewRate(float rateInSeconds)
		{
			SFM_ASSERT(rateInSeconds >= kMinSandHSlewRate && rateInSeconds <= kMaxSandHSlewRate)
			m_slewRate = rateInSeconds;
		}

		void Reset()
		{
			m_prevGate = -1.f;
			m_curSignal.Set(0.f);
		}

	private:
		// Parameters
		/* const */ unsigned m_sampleRate;

		float m_slewRate = kDefSandHSlewRate;

		// State
		float m_prevGate = -1.f;
		InterpolatedParameter<kLinInterpolate> m_curSignal;
	};
}
