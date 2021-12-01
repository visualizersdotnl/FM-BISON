
/*
	FM. BISON hybrid FM synthesis -- Flexible analog style ADSR envelope.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"
#include "3rdparty/JUCE/ADSR.h"

namespace SFM
{
	class Envelope
	{
	public:
		Envelope()
		{
			Reset();
		}

		// Full ADSR
		struct Parameters
		{
			// In seconds
			float preAttack;

			// [0..1]
			float attack;
			float decay;
			float sustain; // Level
			float release;

			// In seconds
			float globalMul; // [kEnvMulMin..kEnvMulMin+kEnvMulRange] (synth-global.h or Arturia DX V manual)
			
			// Curvature
			float attackCurve;
			float decayCurve;
			float releaseCurve;
		};

		void Start(const Parameters &parameters, unsigned sampleRate, bool isCarrier, float keyTracking, float velScaling);

		SFM_INLINE void Stop()
		{
			// Kill pre-attack
			m_preAttackSamples = 0;

			if (false == m_isInfinite)
			{
				// Release
				m_ADSR.noteOff();
			}
			else
			{
				// Sustain forever
				m_ADSR.sustain();
			}
		}

		SFM_INLINE void Reset()
		{
			m_preAttackSamples = 0;
			m_ADSR.reset();
			m_isInfinite = false;
		}

		SFM_INLINE float Sample()
		{
			if (0 != m_preAttackSamples)
			{
				// Wait for it..
				--m_preAttackSamples;

				SFM_ASSERT(0.f == m_ADSR.getSample());

				return 0.f;
			}
			else
			{
				return m_ADSR.getNextSample();
			}
		}

		// Use to get value without sampling
		SFM_INLINE float Get() const
		{
			return m_ADSR.getSample();
		}

		SFM_INLINE bool IsReleasing() const
		{
			const bool isReleasing = m_ADSR.isReleasing();
			return isReleasing;
		}

		SFM_INLINE bool IsIdle() const
		{
			const bool isIdle = false == m_ADSR.isActive();
			SFM_ASSERT(false == isIdle || (true == isIdle && 0.f == m_ADSR.getSample()));
			return isIdle;
		}

		SFM_INLINE bool IsInfinite()
		{
			return m_isInfinite;
		}

		// To emulate piano-style sustain pedal behaviour
		void OnPianoSustain(float falloff, float pedalReleaseMul);

	private:
		unsigned m_preAttackSamples;
		ADSR m_ADSR;
		bool m_isInfinite;
	};
}
