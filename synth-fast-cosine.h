
/*
	FM. BISON hybrid FM synthesis -- Fast (co)sine.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Period is [0..1]
*/

#pragma once

namespace SFM
{
	void InitializeFastCosine();

	// Period [0..1]
	float fast_cosf(float x);
	
	SFM_INLINE static float fast_sinf(float x) { return fast_cosf(fmodf(x-0.25f, 1.f)); } // FIXME: remove fmodf()?
	SFM_INLINE static float fast_tanf(float x) { return fast_sinf(x)/fast_cosf(x);      }
};
