	
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
					// aliasing going on but that's part of the charm
					
					// Filter parameters
					const float filterFreq    = m_phases[3].GetFrequency(); // Fundamental harmonic
					constexpr double filterQ  = 0.314; // kSVFMinFilterQ
					const unsigned sampleRate = GetSampleRate();

					// Get amplitudes
					const float sideMix = m_supersaw.GetSideMix();
					const float mainMix = m_supersaw.GetMainMix();

					// Generate saws (band-limited)
					float saws[kNumSupersawOscillators];
					saws[0] = oscPolySaw(phase, pitch) * sideMix;
					saws[1] = oscPolySaw(m_phases[1].Sample(), m_phases[1].GetPitch()) * sideMix;
					saws[2] = oscPolySaw(m_phases[2].Sample(), m_phases[2].GetPitch()) * sideMix;
					saws[3] = oscPolySaw(m_phases[3].Sample(), m_phases[3].GetPitch()) * mainMix;
					saws[4] = oscPolySaw(m_phases[4].Sample(), m_phases[4].GetPitch()) * sideMix;
					saws[5] = oscPolySaw(m_phases[5].Sample(), m_phases[5].GetPitch()) * sideMix;
					saws[6] = oscPolySaw(m_phases[6].Sample(), m_phases[6].GetPitch()) * sideMix;


/*
					// Generate saws (naive, as documented in thesis)
					float saws[kNumSupersawOscillators];
					saws[0] = oscSaw(phase) * sideMix;
					saws[1] = oscSaw(m_phases[1].Sample()) * sideMix;
					saws[2] = oscSaw(m_phases[2].Sample()) * sideMix;
					saws[3] = oscSaw(m_phases[3].Sample()) * mainMix;
					saws[4] = oscSaw(m_phases[4].Sample()) * sideMix;
					saws[5] = oscSaw(m_phases[5].Sample()) * sideMix;
					saws[6] = oscSaw(m_phases[6].Sample()) * sideMix;
*/

					// Filter saws below fundamental freq.
					m_HPF[0].updateHighpassCoeff(filterFreq, filterQ, sampleRate);
					m_HPF[0].tickMono(saws[0]);
					m_HPF[1].updateCopy(m_HPF[0]);
					m_HPF[1].tickMono(saws[1]);
					m_HPF[2].updateCopy(m_HPF[0]);
					m_HPF[2].tickMono(saws[2]);
					m_HPF[3].updateCopy(m_HPF[0]);
					m_HPF[3].tickMono(saws[3]);
					m_HPF[4].updateCopy(m_HPF[0]);
					m_HPF[4].tickMono(saws[4]);
					m_HPF[5].updateCopy(m_HPF[0]);
					m_HPF[5].tickMono(saws[5]);
					m_HPF[6].updateCopy(m_HPF[0]);
					m_HPF[6].tickMono(saws[6]);

					// Accumulate saws
					for (auto saw : saws)
						signal += saw;
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
