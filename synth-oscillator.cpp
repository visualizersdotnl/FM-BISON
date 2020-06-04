	
/*
	FM. BISON hybrid FM synthesis -- Oscillator (DCO/LFO).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-oscillator.h" 
#include "synth-distort.h"

namespace SFM
{
	// Called *once* by Bison::Bison()
	/* static */ void Oscillator::CalculateSupersawDetuneTable()
	{	
		for (unsigned iSaw = 0; iSaw < kNumPolySupersaws; ++iSaw)
		{
			s_supersawDetune[iSaw] = powf(2.f, (kPolySupersawDetune[iSaw]*0.01f)/12.f);
		}
	}

	/* static */ alignas(16) float Oscillator::s_supersawDetune[kNumPolySupersaws] = { 0.f };

	float Oscillator::Sample(float phaseShift)
	{
		const float phase = m_phases[0].Sample();
		const double pitch = m_phases[0].GetPitch(); // For PolyBLEP

		// FIXME: skip fmodf() if 'phaseShift' is zero?
		const float modulated = fmodf(phase+phaseShift, 1.f);
		
		// This switch statement has never shown up during profiling
		float signal = 0.f;
		switch (m_form)
		{
			case kStatic:
				signal = 0.f;
				break;

			/* Bandlimited (DCO/LFO) */

			case kSine:
				signal = oscSine(modulated);
				break;
					
			case kCosine:
				signal = oscCos(modulated);
				break;
				
			case kPolyTriangle:
				signal = oscPolyTriangle(modulated, pitch);
				break;

			case kPolySquare:
				signal = oscPolySquare(modulated, pitch);
				break;

			case kPolySaw:
				signal = oscPolySaw(modulated, pitch);
				break;

			case kPolyRamp:
				signal = oscPolyRamp(modulated, pitch);
				break;

			case kPolySupersaw:
				{
					// Modulation & (incoming) feedback ignored; they would only result in noise for this oscillator
					const float subGain = 0.354813397f; // -9dB
	
					for (unsigned iSaw = 0; iSaw < kNumPolySupersaws; ++iSaw)
					{
						auto &phaseObj = m_phases[iSaw];
						signal += oscPolySaw(phaseObj.Sample(), 0.33*phaseObj.GetPitch())*subGain;
					}
				}

				break;

			case kPolyRectSine:
				signal = oscPolyRectifiedSine(modulated, pitch);
				break;

			case kPolyTrapezoid:
				signal = oscPolyTrapezoid(modulated, pitch);
				break;

			/* Noise */
				
			case kPinkNoise:
				signal = m_pinkOsc.Sample();
				break;

			/* LFO */

			case kSampleAndHold:
				signal = m_SandH.Sample(modulated, oscWhiteNoise());
				break;

			case kRamp:
				signal = oscRamp(modulated);
				break;

			case kSaw:
				signal = oscSaw(modulated);
				break;

			case kSquare:
				signal = oscSquare(modulated);
				break;

			case kFakeSquare:
				// Quick (unused, 04/06/2020) hack (FIXME)
				signal = Squarepusher(oscSine(modulated), 0.3f);
				break;

			case kFakeRamp:
				// Quick hack (FIXME)
				signal = oscSine(modulated + (1.f + 0.075f*oscPolyTriangle(modulated, pitch)));
				break;

			case kTriangle:
				signal = oscTriangle(modulated);
				break;
			
			// Not implemented
			default:
				signal = oscWhiteNoise();
				SFM_ASSERT(false);
				break;
		}
		
		// I'd like to check the range here as well ([-1..1]) but for ex. pink noise or the supersaw overshoot (FIXME)
		FloatAssert(signal);

		m_signal = signal;
		return signal;
	}
}
