
/*
	FM. BISON hybrid FM synthesis -- Random generator.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

/* Include 'Tiny Mersenne-Twister' 32-bit & 64-bit in-place */
#include "../3rdparty/tinymt/tinymt32.c"
#include "../3rdparty/tinymt/tinymt64.c"

#include "synth-random.h"

namespace SFM
{
	static tinymt32_t s_genState32;
	static tinymt64_t s_genState64;

	void InitializeRandomGenerator()
	{
		tinymt32_init(&s_genState32, rand());
		tinymt64_init(&s_genState64, rand());
	}

	double mt_rand()
	{
		return tinymt64_generate_doubleOO(&s_genState64);
	}

	float mt_randf()
	{
		return tinymt32_generate_floatOO(&s_genState32);
	}

	uint32_t mt_randu32()
	{
		return tinymt32_generate_uint32(&s_genState32);
	}

	int32_t mt_rand32() 
	{ 
		return (int32_t) mt_randu32();
	}
}
