
/*
	FM. BISON hybrid FM synthesis -- Oscillator (VCO/LFO).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- I'm not happy about Oscillator containing specific state and multiple phase objects just to
		  support a handful of special cases
		- https://github.com/bipolaraudio/FM-BISON/issues/84
*/

#pragma once

#pragma warning(push)
#pragma warning(disable: 4324) // Tell MSVC to shut it about padding I'm aware of

#include "synth-global.h"
#include "synth-phase.h"
#include "synth-stateless-oscillators.h"
#include "synth-pink-noise.h"
#include "synth-sample-and-hold.h"

namespace SFM
{
	// Number or oscillators in unison
	// - "According to Duda, '7' Unison is the sweet spot for stacks/chords." (Serum developer)
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
		// Supported waveforms
		enum Waveform
		{
			kStatic,

			// Band-limited
			kSine,
			kCosine,
			kPolyTriangle,
			kPolySquare,
			kPolySaw,
			kPolyRamp,
			kPolyRectifiedSine,
			kPolyTrapezoid,
			kPolyRectangle,
			kBump,

			// Very soft approximation of ramp & saw (for LFO)
			kSoftRamp,
			kSoftSaw,
			
			// Supersaw
			kSupersaw,

			// Raw/LFO
			kRamp,
			kSaw,
			kSquare,
			kTriangle,
			kPulse,

			// Noise
			kWhiteNoise,
			kPinkNoise,

			// S&H (for LFO)
			kSampleAndHold
		};

		// Called once by Bison::Bison()
		static void CalculateSupersawDetuneTable();

	private:
		/* const */ Waveform m_form;
		alignas(16) Phase m_phases[kNumPolySupersaws];

		// Oscillators with state
		PinkNoise     m_pinkNoise;
		SampleAndHold m_sampleAndHold;

		// Signal
		float m_signal = 0.f;
		
		alignas(16) static float s_supersawDetune[kNumPolySupersaws];
		
	public:
		Oscillator(unsigned sampleRate = 1) :
			m_sampleAndHold(sampleRate)
		{
			Initialize(kStatic, 0.f, sampleRate, 0.f);
		}

		void Initialize(Waveform form, float frequency, unsigned sampleRate, float phaseShift)
		{
			m_form = form;
			m_phases[0].Initialize(frequency, sampleRate, phaseShift);
			
			m_pinkNoise     = PinkNoise();
			m_sampleAndHold = SampleAndHold(sampleRate);
			
			if (kSupersaw == m_form)
			{
				// Instead of spawning multiple (detuned) voices I opted for multiple phase objects
				// which is about as good as it gets performance wise

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
			if (1.f == bend)
				return;

			m_phases[0].PitchBend(bend);		
	
			if (kSupersaw == m_form)
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
			if (GetFrequency() == frequency)
				return;

			m_phases[0].SetFrequency(frequency);

			if (kSupersaw == m_form)
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

		SFM_INLINE void SetSampleAndHoldSlewRate(float rate)
		{
			m_sampleAndHold.SetSlewRate(rate);
		}

		SFM_INLINE void SetHardSync(float frequency)
		{
			SFM_ASSERT(kSupersaw != m_form);
			m_phases[0].SyncTo(frequency);
		}
		
		SFM_INLINE float    GetFrequency()   const { return m_phases[0].GetFrequency();  }
		SFM_INLINE unsigned GetSampleRate()  const { return m_phases[0].GetSampleRate(); }
		SFM_INLINE float    GetPhase()       const { return m_phases[0].Get();           }
		SFM_INLINE Waveform GetWaveform()    const { return m_form;                      }

		float Sample(float phaseShift);
	};
}

#pragma warning(pop)
