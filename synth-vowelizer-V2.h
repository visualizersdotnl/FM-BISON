
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"

namespace SFM
{
	class VowelizerV2
	{
	public:
		enum Vowel
		{
			kA,
			kNumVowels
		};

	public:
		VowelizerV2()
		{
			Reset();
		}

		SFM_INLINE void Reset()
		{
		}

		SFM_INLINE float Apply(float sample, Vowel vowel)
		{
			return 0.f;
		}
	};
}
