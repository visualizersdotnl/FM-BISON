	
/*
	FM. BISON hybrid FM synthesis -- Oscillator (DCO/LFO).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-oscillator.h" 

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

		// FIXME: try to skip fmodf() if certain conditions are met
		const float modulated = fmodf(phase+phaseShift, 1.f);

		// PolyBLEP "width"
		const float DT = GetFrequency()/GetSampleRate();
		
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
				signal = oscPolyTriangle(modulated, DT);
				break;

			case kPolySquare:
				signal = oscPolySquare(modulated, DT);
				break;

			case kPolySaw:
				signal = oscPolySaw(modulated, DT);
				break;

			case kPolySupersaw:
				{
					// Modulation & feedback ignored!
					// I *could* fix this but it'd be computationally expensive and doesn't fit the oscillator type anyway.

					const float polyWidthScale = 0.33f;
					const float subGain = 0.354813397f; // -9dB
					const unsigned sampleRate = GetSampleRate();
	
					for (unsigned iSaw = 0; iSaw < kNumPolySupersaws; ++iSaw)
					{
						auto &phaseObj = m_phases[iSaw];
						const float subPolyWidth = phaseObj.GetFrequency()/sampleRate;
						signal += oscPolySaw(phaseObj.Sample(), subPolyWidth)*subGain;
					}
				}

				break;

			case kPolyRectSine:
				signal = oscPolyRectifiedSine(modulated, DT);
				break;

			/* Noise */
				
			case kPinkNoise:
				signal = m_pinkOsc.Sample();
				break;

			/* LFO */

			case kSampleAndHold:
				signal = m_SandH.Sample(modulated, m_pinkOsc.Sample());
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

			case kTriangle:
				signal = oscTriangle(modulated);
				break;
			
			// Unused/Unimplemented oscillators
			default:
				signal = oscWhiteNoise();

				SFM_ASSERT(false);

				break;
		}

		// Can not check for range here (because of, for example, kPolySupersaw)
		FloatAssert(signal);

		return signal;
	}
}
