
/*
	FM. BISON hybrid FM synthesis -- Fast (co)sine.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Period is [0..1], values outside of [0..1] work fine
	- Includes sinus and tangens
*/

#pragma once

#include "../synth-global.h"

#define FAST_COSF_INLINE

namespace SFM
{
	constexpr unsigned kFastCosTabLog2Size = 10; // Equals size of 1024
	constexpr unsigned kFastCosTabSize = 1 << kFastCosTabLog2Size;

	extern double g_fastCosTab[kFastCosTabSize+1];

	void InitializeFastCosine();

#if defined(FAST_COSF_INLINE)

	SFM_INLINE static float fast_cosf(float x)
	{
		// Cosine is symmetrical around 0, let's get rid of negative values
		x = fabs(x);

		auto phase = 1.0 + x;

		const auto phaseAsInt = *reinterpret_cast<unsigned long long *>(&phase);

		const int exponent = (phaseAsInt >> 52) - 1023;

		const auto fractBits  = 32-kFastCosTabLog2Size;
		const auto fractScale = 1 << fractBits;
		const auto fractMask  = fractScale-1;

		const unsigned significant = unsigned(((phaseAsInt<<exponent) >> (52-32)));

		const auto index    = significant >> fractBits;
		const int  fraction = significant &  fractMask;

		const auto left  = g_fastCosTab[index];
		const auto right = g_fastCosTab[index+1];

		const auto fractMix = fraction * (1.0/fractScale);
		return float(left + (right-left)*fractMix);
	}

#else

	float fast_cosf(float x);

#endif

	SFM_INLINE static float fast_sinf(float x)
	{ 
		return fast_cosf(x-0.25f); 
	}

	// FIXME: move to synth-fast-tan.h?
	SFM_INLINE static float fast_tanf(float x) 
	{ 
		return fast_sinf(x)/fast_cosf(x); 
	}
};
