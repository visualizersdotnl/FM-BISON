
/*
	FM. BISON hybrid FM synthesis -- S&H oscillator (or rather a filter).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "../FM-BISON-internal-plug-in/JuceLibraryCode/JuceHeader.h"

#include "synth-global.h"
#include "synth-stateless-oscillators.h"
#include "synth-phase.h"

namespace SFM
{
	class SampleAndHold
	{
	public:
		SampleAndHold(unsigned sampleRate) :
			m_sampleRate(sampleRate)
		{
			Reset();
		}

		~SampleAndHold() {}

		SFM_INLINE float Sample(double phase, float input)
		{
			const float curGate = oscBox(phase);
			
			// Gate?
			if (m_prevGate != curGate)
			{
				// Update slew rate and set input as new target
				m_curSignal.reset(m_sampleRate, m_slewRate);
				m_curSignal.setTargetValue(input);
			}

			m_prevGate = curGate;

			// Slew towards target input
			return Clamp(m_curSignal.getNextValue()); // Clamp to gaurantee output range (FIXME)
		}

		void SetSlewRate(float rateInSeconds)
		{
			SFM_ASSERT(rateInSeconds >= kMinSandHSlewRate && rateInSeconds <= kMaxSandHSlewRate)
			m_slewRate = rateInSeconds;
		}

		void Reset()
		{
			m_prevGate = -1.f;
			m_curSignal.setCurrentAndTargetValue(0.f);
		}

	private:
		// Parameters
		/* const */ unsigned m_sampleRate;

		float m_slewRate = kDefSandHSlewRate;

		// State
		float m_prevGate = -1.f;
		SmoothedValue<float, ValueSmoothingTypes::Linear> m_curSignal;
	};
}
