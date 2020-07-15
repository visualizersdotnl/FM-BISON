
/*
	FM. BISON hybrid FM synthesis -- Random generator.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "../synth-global.h"

namespace SFM
{
	void InitializeRandomGenerator();

	/*
		rand()    -- Returns double prec. random value which is always between 0.0 or 1.0
		randf()   -- Returns single prec. random value which is always between 0.f and 1.f
		randu32() -- Unsigned 32-bit.
		rand32()  -- Signed 32-bit.
		randfc()  -- Returns FP random value between -1 and 1.
	*/

	double mt_rand();
	float mt_randf();
	uint32_t mt_randu32();
	int32_t mt_rand32();

	SFM_INLINE static float mt_randfc()
	{
		return -1.f + 2.f*mt_randf();
	}
};
