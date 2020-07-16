
/*
	FM. BISON hybrid FM synthesis -- Self-contained JP-8000 supersaw oscillator.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Information: https://pdfs.semanticscholar.org/1852/250068e864215dd7f12755cf00636868a251.pdf (copy in repository)
	- Fully double precision
	
	FIXME:
		- Fix beating
		- Review filter
		- Reconsider single precision
*/

#pragma once

#include "3rdparty/filters/Biquad.h"

#include "synth-global.h"
#include "synth-stateless-oscillators.h"
#include "synth-one-pole-filters.h"

namespace SFM
{
	// 7 oscillators, so iOsc = [0..6]
	constexpr unsigned kNumSupersawOscillators = 7;

	// Relation between frequencies (slightly asymmetric)
	// Centre oscillator moved from position 4 to 1
	constexpr double kSupersawRelative[kNumSupersawOscillators] = 
	{
		// According to Adam Szabo
		 0.0, 
		-0.11002313, 
		-0.06288439, 
		-0.01952356,
		 0.10745242,
		 0.06216538, 
		 0.01991221 

/*
		// According to Alex Shore
		 0.0,
		-0.11002313,
		-0.06288439,
		-0.03024148, 
		 0.02953130,
		 0.06216538,
		 0.10745242
*/
	};

	class Supersaw
	{
	private:
		class DCBlocker
		{
		public:
			DCBlocker() {}
			~DCBlocker() {}

			SFM_INLINE double Apply(double sample)
			{
				constexpr double R = 0.995;
				const double out = sample - m_prevSample + R*m_feedback;

				m_prevSample = sample;
				m_feedback = out;

				return out;
			}

		private:
			double m_prevSample = 0.0;
			double m_feedback   = 0.0;
		};

	public:
		Supersaw() : 
			m_sampleRate(1) 
		{ 
			// Yields a soft 'pop'
			Initialize(0.f, 1, 0.0, 0.0, 0.0);
		}
		
		~Supersaw() {}

		void Initialize(float frequency, unsigned sampleRate, double, double detune, double mix)
		{
			m_sampleRate = sampleRate;
			
			// Set JP-8000 controls
			SetDetune(detune);
			SetMix(mix);

			// Reset filter
			m_HPF.reset();

			// Set phases
			for (auto &phase : m_phase)
				phase = mt_rand();

			// Set frequency, pitch, phases & filter
			m_frequency = 0.f; 
			SetFrequency(frequency);
		}

		SFM_INLINE void SetFrequency(float frequency)
		{	
			// Bail if unnecessary; this is relatively expensive
			if (m_frequency != frequency)
			{
				m_frequency = frequency;
				
				for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
				{
					// Pitch
					const double relative = kSupersawRelative[iOsc];
					const double freqOffs = frequency*(m_curDetune*kSupersawRelative[iOsc]);
					const double detuned  = frequency + freqOffs;
					const double pitch    = CalculatePitch<double>(detuned, m_sampleRate);
					m_pitch[iOsc] = pitch;
				}

				// Set HPF
				constexpr double Q = kDefGainAtCutoff;
				m_HPF.setBiquad(bq_type_highpass, m_frequency/m_sampleRate, Q, 0.0);
			}
		}

		SFM_INLINE void PitchBend(double bend)
		{
			// FIXME: implement
		}

		SFM_INLINE float Sample()
		{
			// Centre oscillator
			const double main = Oscillate(0);

			// Side oscillators
			double sides = 0.0;
			for (unsigned iOsc = 1; iOsc < kNumSupersawOscillators; ++iOsc)
				sides += Oscillate(iOsc);

			double signal = m_HPF.process(main*m_mainMix + sides*m_sideMix);
			signal = m_blocker.Apply(signal);

			return float(signal);
		}

		float GetFrequency() const
		{	
			// Fundamental freq.
			return m_frequency;
		}

		float GetPhase() const
		{
			// Return main oscillator's phase
			return float(m_phase[0]);
		}

	private:
		unsigned m_sampleRate;
		float m_frequency = 0.f;

		double m_curDetune = 0.0;
		double m_mainMix   = 0.0; 
		double m_sideMix   = 0.0;

		double m_phase[kNumSupersawOscillators] = { 0.0 };
		double m_pitch[kNumSupersawOscillators] = { 0.0 };

		DCBlocker m_blocker;
		Biquad m_HPF;

		void SetDetune(double detune /* [0..1] */);
		void SetMix(double mix /* [0..1] */);

		SFM_INLINE double Oscillate(unsigned iOsc)
		{
			SFM_ASSERT(iOsc < kNumSupersawOscillators);

			double &phase = m_phase[iOsc];
			double  pitch = m_pitch[iOsc];

			phase += pitch;

			if (phase > 1.0)
				phase -= 1.0;

			SFM_ASSERT(phase >= 0.0 && phase <= 1.0);

			return Saw(phase, pitch);
		}

		// By Tale: http://www.kvraudio.com/forum/viewtopic.php?t=375517
		SFM_INLINE double PolyBLEP(double point, double dT) 
		{
			if (point < dT)
			{
				// Discontinuities between 0 & 1
				point /= dT;
				return point+point - point*point - 1.0;
			}			
			else if (point > 1.0-dT)
			{
				// Discontinuities between -1 & 0
				point = (point-1.0)/dT;
				return point*point + point+point + 1.0;
			}
			else
				return 0.0;
		}

		SFM_INLINE static int64_t bitwiseOrZero(double value) 
		{
			return static_cast<int64_t>(value) | 0;
		}

		SFM_INLINE double Saw(double phase, double pitch)
		{
//			return oscSaw(float(phase));

			double P1 = phase + 0.5;
			P1 -= bitwiseOrZero(P1);

			double saw = 2.0*P1 - 1.0;
			saw -= PolyBLEP(P1, pitch);

			return saw;
		}
	};
}
