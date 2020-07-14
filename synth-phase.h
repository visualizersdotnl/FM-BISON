
/*
	FM. BISON hybrid FM synthesis -- Phase container & logic; basically an oscillator without a specific waveform.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Phase period is [0..1]
	- Uses double precision internally
*/

#pragma once

#include "synth-global.h"
#include "synth-stateless-oscillators.h"

namespace SFM
{
	class Phase
	{
	private:
		double   m_frequency;
		unsigned m_sampleRate;
		double   m_pitch;
		double   m_phase;

	public:
		Phase()
		{
			Initialize(1.f, 44100);
		}

		Phase(unsigned sampleRate) 
		{ 
			Initialize(1.f, sampleRate); 
		}

		SFM_INLINE void Initialize(double frequency, unsigned sampleRate, double phaseShift = 0.0)
		{
			m_frequency = frequency;
			m_sampleRate = sampleRate;
			m_pitch = CalculatePitch(frequency, sampleRate);
			m_phase = fmod(phaseShift, 1.0);

			SFM_ASSERT(m_phase >= 0.0 && m_phase <= 1.0);
		}

		SFM_INLINE void Reset()
		{
			m_phase = 0.0;
		}

		SFM_INLINE void PitchBend(float bend)
		{
			SFM_ASSERT(0.f != bend);
			m_pitch = CalculatePitch(m_frequency*bend, m_sampleRate);
		}

		SFM_INLINE void SetFrequency(double frequency)
		{
			m_pitch = CalculatePitch(frequency, m_sampleRate);
			m_frequency = frequency;
		}

		SFM_INLINE float    GetFrequency()    const { return float(m_frequency);  }
		SFM_INLINE unsigned GetSampleRate()   const { return m_sampleRate;        }
		SFM_INLINE double   GetPitch()        const { return m_pitch;             }
		SFM_INLINE double   Get()             const { return m_phase;             }

		SFM_INLINE double Sample()
		{
			if (m_phase >= 1.0)
			{
				m_phase -= 1.0;
			}
			
			const double curPhase = m_phase;
			SFM_ASSERT(curPhase >= 0.0 && curPhase <= 1.0);

			m_phase += m_pitch;
			
			return curPhase;
		}

		SFM_INLINE void Skip(unsigned count)
		{
			m_phase = fmod(m_phase + m_pitch*count, 1.0);
		}
	};
}
