
/*
	FM. BISON hybrid FM synthesis -- Supersaw utility class (handles JP-8000 approximate calculations).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	See .CPP for details
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	// 7 oscillators, so iOsc = [0..6]
	constexpr unsigned kNumSupersawOscillators = 7;

	class Supersaw
	{
	public:
		Supersaw() {}
		~Supersaw() {}

		void SetDetune(float detune /* [0..1] */);
		
		SFM_INLINE void SetMix(float mix /* [0..1] */)
		{
			SFM_ASSERT(mix >= 0.f && mix <= 1.f);
			m_mainMix = -0.553366f*mix + 0.99785f;
			m_sideMix = -0.73764f*powf(mix, 2.f) + 1.2841f*mix + 0.044327f;
		}

		SFM_INLINE double CalculateDetunedFreq(unsigned iOsc, float frequency) const
		{
			SFM_ASSERT(iOsc < kNumSupersawOscillators);
			SFM_ASSERT(frequency >= 0.f);

			// Relation between frequencies (slightly asymmetric)
			const double kRelations[kNumSupersawOscillators] = 
			{
				-0.11002313, 
				-0.06288439, 
				-0.01952356, 
				 0.0, 
				 0.01991221, 
				 0.06216538, 
				 0.10745242
			};

			return (1.0 + m_curDetuneCurve*kRelations[iOsc])*frequency;
		}

		SFM_INLINE float GetAmplitude(unsigned iOsc) const
		{
			SFM_ASSERT(iOsc < kNumSupersawOscillators);
			
			return (3 == iOsc)
			 ? m_mainMix
			 : m_sideMix;
		}

	private:
		double m_curDetuneCurve = 0.0;
		float m_mainMix = 0.f, m_sideMix = 0.f;
	};
}
