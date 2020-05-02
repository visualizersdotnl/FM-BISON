
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Notes:
		- It's monaural
		- Coarse way of implementing a formant filter (which usually consists of multiple carefully tuned band pass filters)
		- Effective enough for adding a bit of flair to effects
		- Not equal 

	Source: a contribution to http://www.musicdsp.org by alex@smartelectronix.com
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	class Vowelizer
	{
	public:
		enum Vowel
		{
			kI,
			kA,
			kE,
			kO,
			kU,
			kNumVowels
		};

	private:
		alignas(16) double m_rings[2][10];

		float Calculate(float sample, Vowel vowelA, unsigned iRing);

	public:
		Vowelizer()
		{
			Reset();
		}

		SFM_INLINE void Reset()
		{
			memset(m_rings[0], 0, 10 * sizeof(double));
			memset(m_rings[1], 0, 10 * sizeof(double));
		}

		/*
			Notes:
				- Do not mix both Apply() functions without calling Reset() first!
				- Reduce input signal by approx. -3dB (quirky coefficients)
		*/

		SFM_INLINE float Apply(float sample, Vowel vowel)
		{
			return Calculate(sample, vowel, 0);	
		}
		
		// Interpolates to next vowel (wraps around)
		SFM_INLINE float Apply(float sample, Vowel vowelA, float delta)
		{
			SFM_ASSERT(delta >= 0.f && delta <= 1.f);

			const Vowel vowelB = Vowel((vowelA+1) % kNumVowels);
			return lerpf<float>(Calculate(sample, vowelA, 0), Calculate(sample, vowelB, 1), delta);
		}
	};
}
