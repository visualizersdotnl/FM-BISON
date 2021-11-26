	
/*
	FM. BISON hybrid FM synthesis -- Oscillator (DCO/LFO).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-oscillator.h" 
#include "synth-distort.h"

namespace SFM
{
	void Oscillator::Initialize(Waveform form, float frequency, unsigned sampleRate, float phaseShift, float supersawDetune /* = 0.f */, float supersawMix /* = 0.f */)
	{
		switch (form)
		{
		case kWhiteNoise:
			m_phase.Initialize(1.f, sampleRate);
			break;

		case kPinkNoise:
			m_pinkNoise = PinkNoise();
			m_phase.Initialize(1.f, sampleRate);
			break;

		case kSupersaw:
			m_supersaw.Initialize(frequency, sampleRate, supersawDetune, supersawMix);
			break;

		case kSampleAndHold:
			m_sampleAndHold = SampleAndHold(sampleRate);

		default:
			m_phase.Initialize(frequency, sampleRate, phaseShift);
		}		

		m_form = form;
	}

	float Oscillator::Sample(float phaseShift)
	{
		constexpr float defaultDuty = 0.25f; // FIXME: parameter?
		
		// These calls are unnecessary for a few waveforms, but as far as they don't show up in a profiler I'll let them be
		const float phase = m_phase.Sample();
		const float pitch = m_phase.GetPitch(); // For PolyBLEP
		
		const float modulated = (0.f == phaseShift) // Not calling fmodf() certainly warrants a comparison and branch
			? phase // Gauranteed to be [0..1]
			: fmodf(phase+fabsf(phaseShift), 1.f); // Using fabsf() works out and on most IEEE-compliant platforms, should be cheap and idiot-proofs
		
		// Calculate signal (switch statement has never shown up during profiling)
		float signal = 0.f;
		switch (m_form)
		{
			case kNone:
				signal = 0.f;
				break;

			/* Supersaw */

			case kSupersaw:
				signal = m_supersaw.Sample();
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
				Approximate a ramp and a saw with a *very* subtle slope (for LFO)
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

			/* See synth-oscillator.h */

			case kUniRamp:
				{
					const float ramp = oscSine(modulated + 0.1f*oscSine(modulated));
					const float squared = Squarepusher(ramp, 0.4f);
					signal = lerpf<float>(ramp, squared, 0.4f);
					signal = 0.5f + signal*0.5f;
				}

//				signal = oscPolyRamp(modulated, pitch);
//				signal = 0.5f + signal*0.5f;
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
