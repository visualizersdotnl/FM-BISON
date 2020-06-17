
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-vowelizer-V1.h"

const double kVowelCoeffs[SFM::VowelizerV1::kNumVowels][11]= 
{
	// A
	{ 
//		8.11044e-06,
		3.11044e-06,
		8.943665402, -36.83889529, 92.01697887, -154.337906, 181.6233289,
		-151.8651235, 89.09614114, -35.10298511, 8.388101016,  -0.923313471
	},

	// E
	{
		4.36215e-06,
		8.90438318, -36.55179099, 91.05750846, -152.422234, 179.1170248,
		-149.6496211, 87.78352223, -34.60687431, 8.282228154, -0.914150747
	},

	// I
	{ 
		3.33819e-06,
		8.893102966, -36.49532826, 90.96543286, -152.4545478, 179.4835618,
		-150.315433, 88.43409371, -34.98612086, 8.407803364, -0.932568035
	},

	// O
	{
		1.13572e-06,
		8.994734087, -37.2084849, 93.22900521, -156.6929844, 184.596544,
		-154.3755513, 90.49663749, -35.58964535, 8.478996281, -0.929252233
	},

	// U
	{
		4.09431e-07,
		8.997322763, -37.20218544, 93.11385476, -156.2530937, 183.7080141,
		-153.2631681, 89.59539726, -35.12454591, 8.338655623, -0.910251753
	}
};

namespace SFM
{
	// FIXME: probably not worth it, smoothstepf() being faster, but David Hoskins is no slouch at picking functions
	SFM_INLINE static double CosineInterpolate(double a, double b, double t)
	{
		t = (1.0-cos(t*kPI))*0.5f;
		return a*(1.0-t) + b*t;
	}

	void VowelizerV1::Apply(float &sampleL, float &sampleR, float vowel)
	{
		sampleL *= 0.5f; // -6dB
		sampleR *= 0.5f; //

		// Calculate interpolated coefficients (check http://www.kvraudio.com/forum/viewtopic.php?=f=33&t=492329)
		SFM_ASSERT(vowel >= 0.f && vowel <= 4.f);

		const unsigned indexA = unsigned(vowel) % kNumVowels;
		const unsigned indexB = indexA+1;

		const float delta = fracf(vowel);

		const double *pA = kVowelCoeffs[indexA];
		const double *pB = kVowelCoeffs[indexB];

		for (unsigned iCoeff = 0; iCoeff < 11; ++iCoeff)
//			m_interpolatedCoeffs[iCoeff] = lerpf<double>(*pA++, *pB++, smoothstepf(delta));
			m_interpolatedCoeffs[iCoeff] = CosineInterpolate(*pA++, *pB++, delta);

//		Log("indexA = " + std::to_string(indexA) + " indexB = " + std::to_string(indexB));
		
		// Apply & store
		sampleL = (float) Calculate(sampleL, 0);
		sampleR = (float) Calculate(sampleR, 1);
	}
}
