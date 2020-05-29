
/*
	FM. BISON hybrid FM synthesis -- Random generator.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	void InitializeRandomGenerator();

	/*
		randf()   -- Returns FP random value between not 0 but epsilon and 1 (convenient in case you want to divide).
		randu32() -- Unsigned 32-bit.
		rand32()  -- Signed 32-bit.
		randfc()  -- Returns FP random value between -1 and 1.
	*/

	float mt_randf();
	uint32_t mt_randu32();
	int32_t mt_rand32();

	SFM_INLINE static float mt_randfc()
	{
		const float value = -1.f + 2.f*mt_randf();
		SFM_ASSERT(value >= -1.f && value <= 1.f);
		return value;
	}
};
