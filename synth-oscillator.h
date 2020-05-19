
/*
	FM. BISON hybrid FM synthesis -- Oscillator (VCO/LFO).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"
#include "synth-stateless-oscillators.h"
#include "synth-phase.h"
#include "synth-MIDI.h"

namespace SFM
{
	// Number of concurrent oscillators, basically
	const unsigned kNumPolySupersaws = 7;

	// Prime number detune values for supersaw (in cents, almost symmetrical)
	const float kPolySupersawDetune[kNumPolySupersaws] = 
	{
		// Base freq.
		  0.f,
		// Right side
		  5.f,
		 11.f,
		 23.f,
		// Left side
		 -5.f,
		-11.f,
		-23.f

		// A few prime numbers: 2, 3, 5, 7, 11, 17, 23, 31, 41, 59, 73
		// It would be kind of cool to support a (global) scale to tweak the supersaw width(s) (FIXME)
	};

	class Oscillator
	{
	public:
		enum Waveform
		{
			// AA
			kSine,
			kCosine,
			kPolyTriangle,
			kPolySquare,
			kPolySaw,
			kPolySupersaw,
			kPolyRectSine,

			// Raw
			kRamp,
			kSaw,
			kSquare,
			kTriangle,
			kPulse,

			// Noise
			kWhiteNoise,
			kPinkNoise
		};

		// Called once by Bison::Bison()
		static void CalculateSupersawDetuneTable();

	private:
		/* const */ Waveform m_form;
		alignas(16) Phase m_phases[kNumPolySupersaws];

		PinkNoiseState m_pinkState;

		// FIXME: wouldn't it be wiser, memory access wise, to have local copies of this?
		//        in that case you could also decide to allocate only the phase objects you need
		alignas(16) static float s_supersawDetune[kNumPolySupersaws];
		
	public:
		Oscillator(unsigned sampleRate = 1)
		{
			// Oscillator will yield 1.0 at phase 0.0
			Initialize(kCosine, 0.f, sampleRate, 0.f);
		}

		void Initialize(Waveform form, float frequency, unsigned sampleRate, float phaseShift)
		{
			m_form = form;
			m_phases[0].Initialize(frequency, sampleRate, phaseShift);
			
			if (kPolySupersaw == m_form)
			{
				// The idea here is that an optimized (FIXME!) way of multiple detuned oscillators handsomely 
				// beats spawning that number of actual voices, much like the original supersaw is a custom oscillator

				// First saw must be at base freq.
				SFM_ASSERT(1.f == s_supersawDetune[0]);

				for (unsigned iSaw = 1; iSaw < kNumPolySupersaws; ++iSaw)
				{
					auto &phase = m_phases[iSaw];
					const float detune = s_supersawDetune[iSaw];
					phase.Initialize(frequency*detune, sampleRate, phaseShift);
					
					// Hard sync. to base freq.
					phase.SyncTo(frequency);
				}
			}
		}

		SFM_INLINE void PitchBend(float bend)
		{
			m_phases[0].PitchBend(bend);		
	
			if (kPolySupersaw == m_form)
			{
				// Set relative to detune (asserted in Initialize())
				for (unsigned iSaw = 1; iSaw < kNumPolySupersaws; ++iSaw)
				{
					auto &phase = m_phases[iSaw];
					phase.PitchBend(bend*s_supersawDetune[iSaw]);
				}
			}
		}

		SFM_INLINE void SetFrequency(float frequency)
		{
			m_phases[0].SetFrequency(frequency);

			if (kPolySupersaw == m_form)
			{
				// Set relative to detune (asserted in Initialize())
				for (unsigned iSaw = 1; iSaw < kNumPolySupersaws; ++iSaw)
				{
					auto &phase = m_phases[iSaw];
					phase.SetFrequency(frequency*s_supersawDetune[iSaw]);
					
					// Hard sync. to base freq.
					phase.SyncTo(frequency);
				}
			}
		}
		
		SFM_INLINE float    GetFrequency()  const { return m_phases[0].GetFrequency();  }
		SFM_INLINE unsigned GetSampleRate() const { return m_phases[0].GetSampleRate(); }
		SFM_INLINE float    GetPhase()      const { return m_phases[0].Get();           }

		SFM_INLINE Waveform GetWaveform() const 
		{ 
			return m_form; 
		}

		float Sample(float modulation, float feedback = 0.f /* Only used by Voice::Render() */);
	};
}
