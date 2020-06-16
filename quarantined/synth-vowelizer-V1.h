
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Nicked from alex@smartelectronix.com (public domain, http://www.musicdsp.org)

	Quarantined because I need to write my own formant filter; I know all the basics and Tammo Hinrichs pointed me towards
	the 'Klatt' algorithm

	Improved:
		- Smooth interpolation between vowels
		- Stereo support
*/

#pragma once

#include "../synth-global.h"

namespace SFM
{
	class VowelizerV1
	{
	public:
		enum Vowel
		{
			kA,
			kE,
			kI,
			kO,
			kU,
			kNumVowels
		};

	private:
		double m_interpolatedCoeffs[11]; // Calculated by Apply()
		double m_ring[2][10];            // Used by Calculate()

		double Calculate(float sample, unsigned iChan);

	public:
		VowelizerV1()
		{
			Reset();

			// Make sure AutoWah stays in range
			static_assert(kMaxWahSpeakVowel < kNumVowels-1);
		}

		void Reset()
		{
			memset(m_ring[0], 0, 10*sizeof(double));
			memset(m_ring[1], 0, 10*sizeof(double));
		}
		
		// Interpolates between vowels ('vowel' range is [0..kNumVowels-1])
		void Apply(float &sampleL, float &sampleR, float vowel);
	};
}
