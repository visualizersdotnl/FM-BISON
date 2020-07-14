	
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
					// The supersaw consists of 7 oscillators; the third oscillator is the 'main' oscillator at the fundamental frequency
					// I apply a HPF to each oscillator to eliminate 'fold back' aliasing beyond this frequency 
					// There will still be some aliasing going on but that's "part of the charm", or so I'm told

					// Get amplitudes
					const float sideMix = m_supersaw.GetSideMix();
					const float mainMix = m_supersaw.GetMainMix();

					// Generate saws (band-limited)
					float saws[kNumSupersawOscillators];
					saws[0] = oscPolySawFaster(phase, pitch) * sideMix;
					saws[1] = oscPolySawFaster(m_phases[1].Sample(), m_phases[1].GetPitch()) * sideMix;
					saws[2] = oscPolySawFaster(m_phases[2].Sample(), m_phases[2].GetPitch()) * sideMix;
					saws[3] = oscPolySawFaster(m_phases[3].Sample(), m_phases[3].GetPitch()) * mainMix;
					saws[4] = oscPolySawFaster(m_phases[4].Sample(), m_phases[4].GetPitch()) * sideMix;
					saws[5] = oscPolySawFaster(m_phases[5].Sample(), m_phases[5].GetPitch()) * sideMix;
					saws[6] = oscPolySawFaster(m_phases[6].Sample(), m_phases[6].GetPitch()) * sideMix;

					// Filter saws below osc. frequency and accumulate
					signal += m_HPF[0].process(saws[0]);
					signal += m_HPF[1].process(saws[1]);
					signal += m_HPF[2].process(saws[2]);
					signal += m_HPF[3].process(saws[3]);
					signal += m_HPF[4].process(saws[4]);
					signal += m_HPF[5].process(saws[5]);
					signal += m_HPF[6].process(saws[6]);
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
