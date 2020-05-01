
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

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
			kA,
			kI,
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

			- Comment @ http:://www.musicdsp.org by Stefan Hallen, verbatim:
			  "Yeah, morphing lineary between the coefficients works just fine. The distortion I only get when not lowering the amplitude of the input. So I lower it :) Larsby, you can approximate filter curves quite easily, check your dsp literature :)"
			- Do not mix both Apply() functions without calling Reset() first!
		*/

		SFM_INLINE float Apply(float sample, Vowel vowel)
		{
			return Calculate(sample, vowel, 0);	
		}

		// Interpolates towards the next vowel (wraps around)
		SFM_INLINE float Apply(float sample, Vowel vowelA, float delta)
		{
			const Vowel vowelB = Vowel( (vowelA+1) % kNumVowels );
			const float result = lerpf<float>(Calculate(sample, vowelA, 0), Calculate(sample, vowelB, 1), delta);
			return result;
		}
	};
}
