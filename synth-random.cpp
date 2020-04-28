
/*
	FM. BISON hybrid FM synthesis -- Random generator.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-random.h"

/* Include 'Tiny Mersenne-Twister' 32-bit right here, since it's the only place we'll be using it. */
#include "3rdparty/tinymt/tinymt32.c"

namespace SFM
{
	static tinymt32_t s_genState;

	void InitializeRandomGenerator()
	{
		const uint32_t seed = 0xbadf00d;
		tinymt32_init(&s_genState, seed);
	}

	float mt_randf()
	{
		return tinymt32_generate_floatOC(&s_genState);
	}

	uint32_t mt_randu32()
	{
		return tinymt32_generate_uint32(&s_genState);
	}

	int32_t mt_rand32() 
	{ 
		return (int32_t) mt_randu32();
	}
}
