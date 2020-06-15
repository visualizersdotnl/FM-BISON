
/*
	FM. BISON hybrid FM synthesis -- Fast (co)sine.
	(C) visualizers.nl & bipolaraudio.nl
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
};
