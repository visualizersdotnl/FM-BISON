	
/*
	FM. BISON hybrid FM synthesis -- Oscillator (DCO/LFO).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-oscillator.h" 
#include "synth-distort.h"

namespace SFM
{
	float Oscillator::Sample(float phaseShift)
	{
		constexpr float defaultDuty = 0.25f;

		const double phase = m_phases[0].Sample();
		const double pitch = m_phases[0].GetPitch(); // For PolyBLEP
		
		// Calling fmodf() certainly warrants a comparison and branch
		const double modulated = (0.f == phaseShift)
			? phase // Gauranteed to be [0..1]
			: fmod(phase+phaseShift, 1.0);
		
		// This switch statement has never shown up during profiling
		float signal = 0.f;
		switch (m_form)
		{
			case kNone:
				signal = 0.f;
				break;

			/* Supersaw */

			case kSupersaw:
				{
					// The supersaw consists of 7 oscillators; the third oscillator is the 'main' oscillator at the fundamental frequency (the first harmonic).
					// I use this frequency to apply a HPF to each oscillator to eliminate 'fold back' aliasing beyond this frequency. There will still be
					// aliasing going on but that's part of the JP-8000 charm, or so I've read :-)

					const float filterFreq    = m_phases[3].GetFrequency(); // Fundamental harmonic
					constexpr double filterQ  = kSVF12dBFalloffQ; // kSVFMinFilterQ;
					const unsigned sampleRate = GetSampleRate();
					
					// First phase & pitch already available
					signal = oscPolySaw(phase, pitch) * m_supersaw.GetAmplitude(0);
//					signal = oscSaw(phase) * m_supersaw.GetAmplitude(0);

					m_HPF[0].updateHighpassCoeff(filterFreq, filterQ, sampleRate);
					m_HPF[0].tickMono(signal);

					for (unsigned iOsc = 1; iOsc < kNumSupersawOscillators; ++iOsc)
					{
						Phase &phaseObj = m_phases[iOsc];
						float saw = oscPolySaw(phaseObj.Sample(), phaseObj.GetPitch()) * m_supersaw.GetAmplitude(iOsc);
//						float saw = oscSaw(phaseObj.Sample()) * m_supersaw.GetAmplitude(iOsc);

						m_HPF[iOsc].updateHighpassCoeff(filterFreq, filterQ, sampleRate);
						m_HPF[iOsc].tickMono(saw);

						signal += saw;
					}
				}

				break;

			/* Band-limited (DCO/LFO) */

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

			case kPolyRectifiedSine:
				signal = oscPolyRectifiedSine(modulated, pitch);
				break;

			case kPolyTrapezoid:
				signal = oscPolyTrapezoid(modulated, pitch);
				break;

			case kPolyRectangle:
				signal = oscPolyRectangle(modulated, pitch, defaultDuty);
				break;

			case kBump:
				signal = Squarepusher(oscSine(modulated), 0.3f);
				break;

			/*
				These 2 functions are a quick hack (FIXME) to approximate a ramp and a saw with a very gentle slope (LFO)
			*/
			
			case kSoftRamp:
				{
					const float ramp = oscSine(modulated + 0.1f*oscSine(modulated));
					const float squared = Squarepusher(ramp, 0.4f);
					signal = lerpf<float>(ramp, squared, 0.4f);
				}
				
				break;

			case kSoftSaw:
				{
					const float saw = oscSine(modulated - 0.1f*oscSine(modulated));
					const float squared = Squarepusher(saw, 0.4f);
					signal = lerpf<float>(saw, squared, 0.4f);
				}
				
				break;

			/* Noise */
			
			case kWhiteNoise:
				signal = oscWhiteNoise();
				break;
			
			case kPinkNoise:
				signal = m_pinkNoise.Sample();
				break;

			/* LFO */

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

			case kPulse:
				signal = oscPulse(modulated, defaultDuty);
				break;

			/* S&H */

			case kSampleAndHold:
				{
					const float random = oscWhiteNoise();
					signal = m_sampleAndHold.Sample(modulated, random);
				}

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
