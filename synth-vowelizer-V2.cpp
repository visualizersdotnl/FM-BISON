
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Nice article on the subject: https://www.soundonsound.com/techniques/formant-synthesis

	This implementation simply band passes the signal in parallel with 3 different frequencies and widths.
*/

#include "synth-vowelizer-V2.h"

namespace SFM
{
	alignas(16) static const double kVowelFrequencies[VowelizerV2::kNumVowels][3] =
	{
		/* EE */ { 270.0, 2300.0, 3000.0 },
		/* OO */ { 300.0,  870.0, 3000.0 },
		/* I  */ { 400.0, 2000.0, 2250.0 },
		/* E  */ { 530.0, 1850.0, 2500.0 },
		/* U  */ { 640.0, 1200.0, 2400.0 },
		/* A  */ { 660.0, 1700.0, 2400.0 }
	};

	void VowelizerV2::Apply(float &left, float &right, Vowel vowel)
	{
		SFM_ASSERT(vowel < kNumVowels);

		const float bandWidth = 150.f; // 100.0, according to the article (link on top) is the avg. male voice
		const float halfBandWidth = bandWidth*0.5f;

		// Filter and store lower frequencies (below half band width)
		float preL = left, preR = right;
		m_preFilter.updateHighpassCoeff(halfBandWidth*0.5, 0.025, m_sampleRate);
		m_preFilter.tick(preL, preR);

		const float lowL = left-preL;
		const float lowR = right-preR;

		const double *frequencies = kVowelFrequencies[vowel];
		const float   magnitude   = (float) sqrt(frequencies[0]*frequencies[0] + frequencies[1]*frequencies[1] + frequencies[2]*frequencies[2]);

		// Apply 3 parallel band passes
		float filteredL = 0.f, filteredR = 0.f;

		for (unsigned iFormant = 0; iFormant < 3; ++iFormant)
		{
			// Grab frequency
			const float frequency = float(frequencies[iFormant]);
			
			// Divide frequency by half of bandwidth and soft clip it to get a pseudo-Q
			const float normalizedQ = fast_tanhf(frequency/halfBandWidth);
			const float Q = 0.5f + 19.5f*smoothstepf(normalizedQ);

			m_filterBP[iFormant].updateCoefficients(frequency, Q, SvfLinearTrapOptimised2::BAND_PASS_FILTER, m_sampleRate);

			float filterL = preL, filterR = preR;
			m_filterBP[iFormant].tick(filterL, filterR);

			// This linear gain simply makes higher frequencies louder
			const float gain = 1.f-(frequency/magnitude);
			filteredL += filterL*gain;
			filteredR += filterR*gain;
		}

		// Mix low end with result
		left  = lowL + filteredL;
		right = lowR + filteredR;
	}
}
