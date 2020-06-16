
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-vowelizer-V2.h"
#include "../synth-distort.h"

namespace SFM
{
	void VowelizerV2::Apply(float &left, float &right, Vowel vowel)
	{
		SFM_ASSERT(vowel < kNumVowels);

		const float bandWidth = 100.f; // 100.0 (Hz), according to the article I read is the avg. male voice
		const float halfBandWidth = bandWidth*0.5f;

		// Filter and store lower frequencies (below half band width)
		float preL = left, preR = right;
		m_preFilter.updateHighpassCoeff(halfBandWidth*0.25, 0.025, m_sampleRate);
		m_preFilter.tick(preL, preR);

		const float lowL = left-preL;
		const float lowR = right-preR;

		const float  *frequencies = kVowelFrequencies[vowel];
		const float   magnitude   = (float) sqrt(frequencies[0]*frequencies[0] + frequencies[1]*frequencies[1] + frequencies[2]*frequencies[2]);

		// Apply 3 parallel band passes
		float filteredL = 0.f, filteredR = 0.f;

		for (unsigned iFormant = 0; iFormant < 3; ++iFormant)
		{
			// Grab frequency
			const float frequency = frequencies[iFormant];
			
			// Calculate Q (higher frequency means wider response)
			const float normalizedFreq = frequency/magnitude;
			const float Q = 0.05f + 39.f*normalizedFreq;

			m_filterBP[iFormant].updateCoefficients(frequency, Q, SvfLinearTrapOptimised2::BAND_PASS_FILTER, m_sampleRate);

			float filterL = preL, filterR = preR;
			m_filterBP[iFormant].tick(filterL, filterR);

			// This linear gain simply makes higher frequencies louder
			const float gain = 1.f-normalizedFreq;
			filteredL += filterL*gain;
			filteredR += filterR*gain;
		}

		// Mix low end with result
		left  = lowL + filteredL;
		right = lowR + filteredR;
	}
}
