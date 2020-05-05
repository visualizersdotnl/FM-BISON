
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Source: see synth-vowelizer.h
*/

#include "synth-vowelizer.h"

namespace SFM
{
	alignas(16) static const double kVowelCoeffs[Vowelizer::kNumVowels][11] = 
	{
		// I
		{ 
			3.33819e-06,
			8.893102966, -36.49532826, 90.96543286, -152.4545478, 179.4835618,
			-150.315433, 88.43409371, -34.98612086, 8.407803364, -0.932568035
		},

		// A
		{ 
			8.11044e-06, // 3.11044e-06
			8.943665402, -36.83889529, 92.01697887, -154.337906, 181.6233289,
			-151.8651235, 89.09614114, -35.10298511, 8.388101016,  -0.923313471
		},

		// U
		{
			4.09431e-07,
			8.997322763, -37.20218544, 93.11385476, -156.2530937, 183.7080141,
			-153.2631681, 89.59539726, -35.12454591, 8.338655623, -0.910251753
		},

		// E
		{
			4.36215e-06,
			8.90438318, -36.55179099, 91.05750846, -152.422234, 179.1170248,
			-149.6496211, 87.78352223, -34.60687431, 8.282228154, -0.914150747
		},

		// O
		{
			1.13572e-06,
			8.994734087, -37.2084849, 93.22900521, -156.6929844, 184.596544,
			-154.3755513, 90.49663749, -35.58964535, 8.478996281, -0.929252233
		}
	};

	float Vowelizer::Calculate(float sample, Vowel vowelA, unsigned iRing)
	{
		SFM_ASSERT(vowelA < kNumVowels);
		SFM_ASSERT(iRing < 2);

		double *buffer = m_rings[iRing];

		const double *vowelCoeffs = kVowelCoeffs[vowelA];

		double result = 0.0;
		result += vowelCoeffs[0]  * sample;
		result += vowelCoeffs[1]  * buffer[0];
		result += vowelCoeffs[2]  * buffer[1];
		result += vowelCoeffs[3]  * buffer[2];
		result += vowelCoeffs[4]  * buffer[3];
		result += vowelCoeffs[5]  * buffer[4];
		result += vowelCoeffs[6]  * buffer[5];
		result += vowelCoeffs[7]  * buffer[6];
		result += vowelCoeffs[8]  * buffer[7];
		result += vowelCoeffs[9]  * buffer[8];
		result += vowelCoeffs[10] * buffer[9];

		buffer[9] = buffer[8];
		buffer[8] = buffer[7];
		buffer[7] = buffer[6];
		buffer[6] = buffer[5];
		buffer[5] = buffer[4];
		buffer[4] = buffer[3];
		buffer[3] = buffer[2];
		buffer[2] = buffer[1];
		buffer[1] = buffer[0];
		buffer[0] = result;

		return float(result);
	}
}

