
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

		SFM_INLINE double Calculate(float sample, unsigned iChan)
		{
			SFM_ASSERT(iChan < 2);

			double *buffer = m_ring[iChan];

			double result;
			result  = m_interpolatedCoeffs[0]  * sample;
			result += m_interpolatedCoeffs[1]  * buffer[0];
			result += m_interpolatedCoeffs[2]  * buffer[1];
			result += m_interpolatedCoeffs[3]  * buffer[2];
			result += m_interpolatedCoeffs[4]  * buffer[3];
			result += m_interpolatedCoeffs[5]  * buffer[4];
			result += m_interpolatedCoeffs[6]  * buffer[5];
			result += m_interpolatedCoeffs[7]  * buffer[6];
			result += m_interpolatedCoeffs[8]  * buffer[7];
			result += m_interpolatedCoeffs[9]  * buffer[8];
			result += m_interpolatedCoeffs[10] * buffer[9];

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

			return result;
		}

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
