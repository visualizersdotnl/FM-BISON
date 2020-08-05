
/*
	FM. BISON hybrid FM synthesis -- Self-contained JP-8000 style supersaw oscillator.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Ref.: https://pdfs.semanticscholar.org/1852/250068e864215dd7f12755cf00636868a251.pdf (copy in repository)
	- Floating point precision either double or single (ham-fisted copied implementation)
	- Free running: all phases are updated by Bison::Render() if the oscillator is not being used
	
	FIXME:
		- Compare double and single precision version in analyzers
		- Minimize beating (especially at lower frequencies)
		- Review filter
		- SSE implementation (will give a boost that eliminates the need for single prec.)

	For now I'm picking the single precision version since I can't hear the f*cking difference ;)
*/

#pragma once

#include "3rdparty/filters/Biquad.h"

#include "synth-global.h"
#include "synth-stateless-oscillators.h"
#include "synth-one-pole-filters.h"

// #define SS_DOUBLE_PREC

namespace SFM
{

#if defined(SS_DOUBLE_PREC) // Full double prec. impl.

	// Number of oscillators
	constexpr unsigned kNumSupersawOscillators = 7;

	// Relation between frequencies (slightly asymmetric)
	// Centre oscillator moved from position 4 to 1
	constexpr double kSupersawRelative[kNumSupersawOscillators] = 
	{
/*
		// According to Adam Szabo
		 0.0, 
		-0.11002313, 
		-0.06288439, 
		-0.01952356,
		 0.01991221,
		 0.06216538, 
		 0.10745242
*/

		// According to Alex Shore
		 0.0,
		-0.11002313,
		-0.06288439,
		-0.03024148, 
		 0.02953130,
		 0.06216538,
		 0.10745242
	};

	class Supersaw
	{
		class DCBlocker
		{
		public:
			void Reset()
			{
				m_prevSample = 0.0;
				m_feedback   = 0.0;
			}

			SFM_INLINE double Apply(double sample)
			{
				constexpr double R = 0.9925; // What "everyone" uses in a leaky integrator is 0.995
				m_feedback = sample-m_prevSample + R*m_feedback;
				m_prevSample = sample;
				return m_feedback;
			}

		private:
			double m_prevSample = 0.0;
			double m_feedback   = 0.0;
		};

	public:
		Supersaw() : 
			m_sampleRate(1) 
		{
			// Initialize phases with random values between [0..1] and let's hope that at least a few of them are irrational
			for (auto &phase : m_phase)
				phase = mt_randf();
		}

		void Initialize(float frequency, unsigned sampleRate, double detune, double mix)
		{
			m_sampleRate = sampleRate;
			
			// Set JP-8000 controls
			SetDetune(detune);
			SetMix(mix);

			// Reset filter
			m_HPF.reset();

			// Reset DC blocker
			m_blocker.Reset();

			// Set frequency (pitch, filter)
			m_frequency = 0.f; 
			SetFrequency(frequency);
		}

		SFM_INLINE void SetFrequency(float frequency)
		{	
			// Bail if unnecessary; this is relatively expensive
			if (m_frequency != frequency)
			{
				m_frequency = frequency;
				
				// Calc. pitch
				for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
				{
					const double offset   = m_curDetune*kSupersawRelative[iOsc];
					const double freqOffs = frequency * offset;
					const double detuned  = frequency + freqOffs;
					const double pitch    = CalculatePitch<double>(detuned, m_sampleRate);
					m_pitch[iOsc] = pitch;
				}

				// Set HPF
				constexpr double Q = kDefGainAtCutoff*kPI*0.5;
				m_HPF.setBiquad(bq_type_highpass, m_frequency/m_sampleRate, Q, 0.0);
			}
		}

		SFM_INLINE void PitchBend(double bend)
		{
			if (1.0 == bend)
				return;

			const double frequency = m_frequency*bend;

			// Calc pitch.
			for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
			{
				const double offset   = m_curDetune*kSupersawRelative[iOsc];
				const double freqOffs = frequency * offset;
				const double detuned  = frequency + freqOffs;
				const double pitch    = CalculatePitch<double>(detuned, m_sampleRate);
				m_pitch[iOsc] = pitch;
			}

			// Set HPF
			constexpr double Q = kDefGainAtCutoff*kPI*0.5;
			m_HPF.setBiquad(bq_type_highpass, frequency/m_sampleRate, Q, 0.0);
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

			return float(signal); // This cast hurts (expensive instruction)
		}
		
		// Advance phase by a number of samples (used by Bison::Render())
		SFM_INLINE void Run(unsigned numSamples)
		{
			for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
			{
				double &phase = m_phase[iOsc];
				phase = fmod(phase + numSamples*m_pitch[iOsc], 1.0);
			}
		}

		SFM_INLINE float GetFrequency() const
		{	
			// Fundamental freq.
			return m_frequency;
		}

		SFM_INLINE float GetPhase() const
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

		Biquad m_HPF;
		DCBlocker m_blocker;

		void SetDetune(double detune /* [0..1] */);
		void SetMix(double mix /* [0..1] */);

		SFM_INLINE double Tick(unsigned iOsc)
		{
			SFM_ASSERT(iOsc < kNumSupersawOscillators);

			double &phase = m_phase[iOsc];
			double  pitch = m_pitch[iOsc];

			const double oscPhase = phase;
			SFM_ASSERT(oscPhase >= 0.0 && oscPhase <= 1.0);

			phase += pitch;

			if (phase > 1.0)
				phase -= 1.0;

			return oscPhase;
		}

		SFM_INLINE double Oscillate(unsigned iOsc)
		{
			return Saw(Tick(iOsc), m_pitch[iOsc]);
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
//			return oscSquare(float(phase));

//			phase += 0.5;
//			return 2.0*phase - 1.0;

			double P1 = phase + 0.5;
			P1 -= bitwiseOrZero(P1);


			double saw = 2.0*P1 - 1.0;
			saw -= PolyBLEP(P1, pitch);

			return saw;
		}
	};

#else

	// Almost single prec. impl.

	// Number of oscillators
	constexpr unsigned kNumSupersawOscillators = 7;

	// Relation between frequencies (slightly asymmetric)
	// Centre oscillator moved from position 4 to 1
	constexpr float kSupersawRelative[kNumSupersawOscillators] = 
	{
/*
		// According to Adam Szabo
		 0.f, 
		-0.11002313f, 
		-0.06288439f, 
		-0.01952356f,
		 0.01991221f,
		 0.06216538f, 
		 0.10745242f
*/

		// According to Alex Shore
		 0.f,
		-0.11002313f,
		-0.06288439f,
		-0.03024148f, 
		 0.02953130f,
		 0.06216538f,
		 0.10745242f
	};

	class Supersaw
	{
		class DCBlocker
		{
		public:
			void Reset()
			{
				m_prevSample = 0.f;
				m_feedback   = 0.f;
			}

			SFM_INLINE float Apply(float sample)
			{
				constexpr float R = 0.9925f; // What "everyone" uses in a leaky integrator is 0.995
				m_feedback = sample-m_prevSample + R*m_feedback;
				m_prevSample = sample;
				return m_feedback;
			}

		private:
			float m_prevSample = 0.f;
			float m_feedback   = 0.f;
		};

	public:
		Supersaw() : 
			m_sampleRate(1) 
		{
			// Initialize phases with random values between [0..1] and let's hope that at least a few of them are irrational
			for (auto &phase : m_phase)
				phase = mt_randf();
		}

		void Initialize(float frequency, unsigned sampleRate, float detune, float mix)
		{
			m_sampleRate = sampleRate;
			
			// Set JP-8000 controls
			SetDetune(detune);
			SetMix(mix);

			// Reset filter
			m_HPF.reset();

			// Reset DC blocker
			m_blocker.Reset();

			// Set frequency (pitch, filter)
			m_frequency = 0.f; 
			SetFrequency(frequency);
		}

		SFM_INLINE void SetFrequency(float frequency)
		{	
			// Bail if unnecessary; this is relatively expensive
			if (m_frequency != frequency)
			{
				m_frequency = frequency;
				
				// Calc. pitch
				for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
				{
					const float offset   = m_curDetune*kSupersawRelative[iOsc];
					const float freqOffs = frequency * offset;
					const float detuned  = frequency + freqOffs;
					const float pitch    = CalculatePitch<float>(detuned, m_sampleRate);
					m_pitch[iOsc] = pitch;
				}

				// Set HPF
				constexpr float Q = kDefGainAtCutoff*kPI*0.5;
				m_HPF.setBiquad(bq_type_highpass, m_frequency/m_sampleRate, Q, 0.0);
			}
		}

		SFM_INLINE void PitchBend(float bend)
		{
			if (1.f == bend)
				return;

			const float frequency = m_frequency*bend;

			// Calc pitch.
			for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
			{
				const float offset   = m_curDetune*kSupersawRelative[iOsc];
				const float freqOffs = frequency * offset;
				const float detuned  = frequency + freqOffs;
				const float pitch    = CalculatePitch<float>(detuned, m_sampleRate);
				m_pitch[iOsc] = pitch;
			}

			// Set HPF
			constexpr float Q = kDefGainAtCutoff*kPI*0.5f;
			m_HPF.setBiquad(bq_type_highpass, frequency/m_sampleRate, Q, 0.0);
		}

		SFM_INLINE float Sample()
		{
			// Centre oscillator
			const float main = Oscillate(0);

			// Side oscillators
			float sides = 0.f;
			for (unsigned iOsc = 1; iOsc < kNumSupersawOscillators; ++iOsc)
				sides += Oscillate(iOsc);

			float signal = m_HPF.processf(main*m_mainMix + sides*m_sideMix);
			signal = m_blocker.Apply(signal);

			return signal;
		}
		
		// Advance phase by a number of samples (used by Bison::Render())
		SFM_INLINE void Run(unsigned numSamples)
		{
			for (unsigned iOsc = 0; iOsc < kNumSupersawOscillators; ++iOsc)
			{
				float &phase = m_phase[iOsc];
				phase = fmodf(phase + numSamples*m_pitch[iOsc], 1.f);
			}
		}

		SFM_INLINE float GetFrequency() const
		{	
			// Fundamental freq.
			return m_frequency;
		}

		SFM_INLINE float GetPhase() const
		{
			// Return main oscillator's phase
			return m_phase[0];
		}

	private:
		unsigned m_sampleRate;
		float m_frequency = 0.f;

		float m_curDetune = 0.f;
		float m_mainMix   = 0.f; 
		float m_sideMix   = 0.f;

		float m_phase[kNumSupersawOscillators] = { 0.f };
		float m_pitch[kNumSupersawOscillators] = { 0.f };

		Biquad m_HPF;
		DCBlocker m_blocker;

		void SetDetune(float detune /* [0..1] */);
		void SetMix(float mix /* [0..1] */);

		SFM_INLINE float Tick(unsigned iOsc)
		{
			SFM_ASSERT(iOsc < kNumSupersawOscillators);

			float &phase = m_phase[iOsc];
			float  pitch = m_pitch[iOsc];

			const float oscPhase = phase;
			SFM_ASSERT(oscPhase >= 0.f && oscPhase <= 1.f);

			phase += pitch;

			if (phase > 1.f)
				phase -= 1.f;

			return oscPhase;
		}

		SFM_INLINE float Oscillate(unsigned iOsc)
		{
			return oscPolySaw(Tick(iOsc), m_pitch[iOsc]);
		}
	};

#endif

}

