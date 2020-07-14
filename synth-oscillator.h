
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

#include "3rdparty/filters/Biquad.h"

#include "synth-global.h"
#include "synth-phase.h"
#include "synth-stateless-oscillators.h"
#include "synth-pink-noise.h"
#include "synth-sample-and-hold.h"
#include "synth-supersaw.h"

namespace SFM
{
	class Oscillator
	{
	public:
		// Supported waveforms
		enum Waveform
		{
			kNone,

			// Band-limited
			kSine,
			kCosine,
			kPolyTriangle,
			kPolySquare,
			kPolySaw,
			kPolyRamp,
			kPolyRectifiedSine,
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

	private:
		/* const */ Waveform m_form;
		Phase m_phases[kNumSupersawOscillators];

		// Oscillators with state
		PinkNoise     m_pinkNoise;
		SampleAndHold m_sampleAndHold;

		// Supersaw utility class & filters
		Supersaw m_supersaw;
		Biquad m_HPF[kNumSupersawOscillators];

		// Signal
		float m_signal = 0.f;

		// Do *not* call this function needlessly
		SFM_INLINE void UpdateSupersawFilters()
		{
			const unsigned sampleRate = GetSampleRate();
			constexpr double Q = kDefGainAtCutoff;

			for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
				m_HPF[iOsc].setBiquad(bq_type_highpass, m_phases[iOsc].GetFrequency()/sampleRate, Q, 0.0);
		}
		
	public:
		Oscillator(unsigned sampleRate = 1) :
			m_sampleAndHold(sampleRate)
		{
			Initialize(kNone, 0.f, sampleRate, 0.0);
		}

		void Initialize(Waveform form, float frequency, unsigned sampleRate, float phaseShift, float supersawDetune = 0.f, float supersawMix = 0.f)
		{
			m_form = form;
			
			m_pinkNoise     = PinkNoise();
			m_sampleAndHold = SampleAndHold(sampleRate);

			if (kSupersaw != m_form)
			{
				m_phases[0].Initialize(frequency, sampleRate, phaseShift);
			}
			else
			{
				m_supersaw.SetDetune(supersawDetune);
				m_supersaw.SetMix(supersawMix);

				for (auto &filter : m_HPF)
					filter.reset();

				for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
				{
					const float detune = m_supersaw.GetDetune(iOsc);
					m_phases[iOsc].Initialize(detune*frequency, sampleRate, mt_randf() /* Important: randomized phases, prevents flanging! */);
				}
				
				UpdateSupersawFilters();
			}
		}

		SFM_INLINE void PitchBend(float bend)
		{
			if (kSupersaw != m_form)
			{
				m_phases[0].PitchBend(bend);
			}
			else
			{
				for (auto &phase : m_phases)
					phase.PitchBend(bend);
			}
		}

		SFM_INLINE void SetFrequency(float frequency)
		{
			if (kSupersaw != m_form)
			{
				m_phases[0].SetFrequency(frequency);
			}
			else
			{
				// This is relatively expensive, so if the 4th oscillator (neutral) equals frequency we should bail
				if (frequency != m_phases[3].GetFrequency())
				{
					for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
					{
						const float detune = m_supersaw.GetDetune(iOsc);
						m_phases[iOsc].SetFrequency(detune*frequency);
					}

					UpdateSupersawFilters();
				}
			}
		}

		SFM_INLINE void SetSampleAndHoldSlewRate(float rate)
		{
			m_sampleAndHold.SetSlewRate(rate);
		}

		SFM_INLINE void Reset()
		{
			for (auto &phase : m_phases)
				phase.Reset();
		}
		
		SFM_INLINE float GetFrequency() const 
		{ 
			return (m_form != kSupersaw)
				? m_phases[0].GetFrequency()
				: m_phases[3].GetFrequency();
		}

		// - Warning: this value *can* be out of bounds! ([0..1])
		// - Useless for kSupersaw
		SFM_INLINE float GetPhase() const 
		{ 
			SFM_ASSERT(m_form != kSupersaw);
			return m_phases[0].Get();           
		} 

		SFM_INLINE unsigned GetSampleRate() const { return m_phases[0].GetSampleRate(); }
		SFM_INLINE Waveform GetWaveform()   const { return m_form;                      }

		float Sample(float phaseShift);
	};
}

#pragma warning(pop)
