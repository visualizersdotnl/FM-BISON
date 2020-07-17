
/*
	FM. BISON hybrid FM synthesis -- Phase container & logic; basically an oscillator without a specific waveform.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Phase period is [0..1]
*/

#pragma once

#include "synth-global.h"
#include "synth-stateless-oscillators.h"

namespace SFM
{
	
	class Phase
	{
	private:
		float    m_frequency;
		unsigned m_sampleRate;
		float    m_pitch;
		float    m_phase;

	public:
		Phase()
		{
			Initialize(1.f, 44100);
		}

		Phase(unsigned sampleRate) 
		{ 
			Initialize(1.f, sampleRate); 
		}

		SFM_INLINE void Initialize(float frequency, unsigned sampleRate, float phaseShift = 0.f)
		{
			m_frequency = frequency;
			m_sampleRate = sampleRate;
			m_pitch = CalculatePitch(frequency, sampleRate);
			m_phase = fmodf(phaseShift, 1.f);

			SFM_ASSERT(m_phase >= 0.f && m_phase <= 1.f);
		}

		SFM_INLINE void Reset()
		{
			m_phase = 0.f;
		}

		SFM_INLINE void PitchBend(float bend)
		{
			SFM_ASSERT(0.f != bend);
			m_pitch = CalculatePitch(m_frequency*bend, m_sampleRate);
		}

		SFM_INLINE void SetFrequency(float frequency)
		{
			m_pitch = CalculatePitch(frequency, m_sampleRate);
			m_frequency = frequency;
		}

		SFM_INLINE float     GetFrequency()    const { return m_frequency;  }
		SFM_INLINE unsigned  GetSampleRate()   const { return m_sampleRate; }
		SFM_INLINE float     GetPitch()        const { return m_pitch;      }
		SFM_INLINE float     Get()             const { return m_phase;      }

		SFM_INLINE float Sample()
		{
			const float curPhase = m_phase;
			SFM_ASSERT(curPhase >= 0.f && curPhase <= 1.f);

			m_phase += m_pitch;

			if (m_phase >= 1.f)
				m_phase -= 1.f;
			
			return curPhase;
		}

		// Can be used for free running phases
		SFM_INLINE void Skip(unsigned count)
		{
			m_phase = fmodf(m_phase + m_pitch*count, 1.f);
		}
	};
}
