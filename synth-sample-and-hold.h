
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

		SFM_INLINE float Sample(float phase, float input, bool skipTrick = false /* Skip MIDI LUT trick and use unaltered input */)
		{
			const float curGate = oscPulse(phase, 0.5f); // oscSquare()
			
			// Gate?
			if (m_prevGate != curGate)
			{
				// Update slew rate and set signal to hold as new target
				const float curSignal = m_curSignal.Sample();
				m_curSignal.SetRate(m_sampleRate, m_slewRate);
				m_curSignal.Set(curSignal);
				
				if (false == skipTrick)
				{
					// This was just a dumb test but in fact it's more pleasing to the ear
					// than just using the (white noise) input signal
					const unsigned index = unsigned(truncf(fabsf(input*127.f)));
					const float noteFreq = float(g_MIDIToFreqLUT[index]);
					input = sinf(noteFreq*k2PI);				
				}

				m_curSignal.SetTarget(input);
			}

			m_prevGate = curGate;

			// Slew towards target signal
			// Clamp to be 100% sure we're within [-1..1]
			return Clamp(m_curSignal.Sample()); 
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
