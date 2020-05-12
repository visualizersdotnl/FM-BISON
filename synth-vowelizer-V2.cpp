
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

		const double bandWidth = 150.0; // 100.0, according to the article (link on top) is the avg. male voice
		const double halfBandWidth = bandWidth/2.0;

		// Filter and store lower frequencies (below half band width)
		float preL = left, preR = right;
		m_preFilter.updateHighpassCoeff(halfBandWidth, 0.5, m_sampleRate);
		m_preFilter.tick(preL, preR);

		const float lowL = left-preL;
		const float lowR = right-preR;

		const double *frequencies = kVowelFrequencies[vowel];

		// Calculate magnitude of frequencies as if it were a 3D vector
		// We're knee deep in pseudo-science from this point onward :-)
		const double magnitude = sqrt(frequencies[0]*frequencies[0] + frequencies[1]*frequencies[1] + frequencies[2]*frequencies[2]);

		// Apply 3 parallel band passes
		float filteredL = 0.f, filteredR = 0.f;

		for (unsigned iFormant = 0; iFormant < 3; ++iFormant)
		{
			// Grab frequency
			const double frequency = frequencies[iFormant];
			
			// So we'll map the normalized frequency on a curve, and then
			// multiply it by frequency to bandwidth ratio; this is even worse
			// than regarding the 3 frequencies as a 3D vector I guess
			const double normalized = frequency/magnitude;
			const double curve = 1.0 + 0.5*tanh(sin(normalized*kPI));
			const double Q = curve*(frequency/bandWidth);

			// The filter's documentation says that Q may not exceed 40.0, but so far so good :)
			m_filterBP[iFormant].updateCoefficients(frequency, Q, SvfLinearTrapOptimised2::BAND_PASS_FILTER, m_sampleRate);

			float filterL = preL, filterR = preR;
			m_filterBP[iFormant].tick(filterL, filterR);

			filteredL += filterL;
			filteredR += filterR;
		}

		// Mix low end with normalized result
		left  = lowL + filteredL;
		right = lowR + filteredR;
	}
}
