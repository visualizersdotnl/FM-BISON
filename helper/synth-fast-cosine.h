
/*
	FM. BISON hybrid FM synthesis -- Fast (co)sine.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Period is [0..1], values outside of [0..1] work fine.
	Includes sinus and tangent.
*/

#pragma once

#include "../synth-global.h"

namespace SFM
{
	void InitializeFastCosine();
	float fast_cosf(float x);

	SFM_INLINE static float fast_sinf(float x)
	{ 
		return fast_cosf(x-0.25f); 
	}

	SFM_INLINE static float fast_tanf(float x) 
	{ 
		return fast_sinf(x)/fast_cosf(x); 
	}
};
