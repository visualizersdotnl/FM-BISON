
/*
	FM. BISON hybrid FM synthesis -- Flexible ADSR envelope.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

// Nice public domain implementation by Nigel Redmon (earlevel.com), adapted and modified
#include "3rdparty/ADSR.h"

#include "synth-global.h"

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
			float rateMul; // [kEnvMulMin..kEnvMulMin+kEnvMulRange] (synth-global.h or Arturia DX V manual)
			
			// Curvature (see https://www.earlevel.com/main/2013/06/23/envelope-generators-adsr-widget/ for a visualization of response)
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
				m_ADSR.gate(false, 0.f);
			}
			else
				// Sustain forever
				m_ADSR.sustain();
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
				return 0.f;
			}
			else
			{
				return m_ADSR.process();
			}
		}

		// Use to get value without sampling
		SFM_INLINE float Get() const
		{
			return m_ADSR.getOutput();
		}

		SFM_INLINE bool IsReleasing() const
		{
			const bool isReleasing = m_ADSR.getState() == ADSR::env_release;
			return isReleasing;
		}

		SFM_INLINE bool IsIdle() const
		{
			const bool isIdle = m_ADSR.getState() == ADSR::env_idle;
			SFM_ASSERT(false == isIdle || (true == isIdle && 0.f == m_ADSR.getOutput()));
			return isIdle;
		}

		SFM_INLINE bool IsInfinite()
		{
			return m_isInfinite;
		}

		// Used to emulate piano-style sustain pedal behaviour (see impl., FM_BISON.cpp & ADSR.h)
		void OnPianoSustain(unsigned sampleRate, float falloff, float releaseRateMul);

	private:
		unsigned m_preAttackSamples;
		ADSR m_ADSR;
		bool m_isInfinite;
	};
}
