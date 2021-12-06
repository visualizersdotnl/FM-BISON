
/*
	FM. BISON hybrid FM synthesis -- Fast (co)sine.
	(C) njdewit technologies (visualizers.nl) & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Supplied by Erik "Kusma" Faye-Lund.
*/

#include "synth-fast-cosine.h"

namespace SFM
{
	alignas(16) double g_fastCosTab[kFastCosTabSize+1];

	void InitializeFastCosine()
	{
		for (unsigned iExp = 0; iExp < kFastCosTabSize+1; ++iExp)
		{
			const double phase = double(iExp) * ((double(k2PI)/kFastCosTabSize));
			g_fastCosTab[iExp] = cos(phase);
		}
	}

#if !defined(FAST_COSF_INLINE)

	float fast_cosf(float x)
	{
		// Cosine is symmetrical around 0, let's get rid of negative values
		x = fabs(x);

		auto phase = 1.0 + x;

		const auto phaseAsInt = *reinterpret_cast<unsigned long long *>(&phase);

		const int exponent = (phaseAsInt >> 52) - 1023;

		const auto fractBits  = 32-kFastCosTabLog2Size;
		const auto fractScale = 1 << fractBits;
		const auto fractMask  = fractScale-1;

		const unsigned significand = unsigned(((phaseAsInt<<exponent) >> (52-32)));

		const auto index    = significand >> fractBits;
		const int  fraction = significand &  fractMask;

		const auto left  = g_fastCosTab[index];
		const auto right = g_fastCosTab[index+1];

		const auto fractMix = fraction * (1.0/fractScale);
		return float(left + (right-left)*fractMix);
	}

#endif
};
