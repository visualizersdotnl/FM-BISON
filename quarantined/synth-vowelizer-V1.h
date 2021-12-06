
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) njdewit technologies (visualizers.nl) & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Nicked from alex@smartelectronix.com (public domain, http://www.musicdsp.org)

	Quarantined because I need to write my own formant filter; I know all the basics and Tammo Hinrichs pointed me towards
	the 'Klatt' algorithm

	Improved:
		- Smooth interpolation between vowels
		- Stereo support

	Important: 
		- Do *not* call more often than than GetSampleRate() per second, the coefficients were designed for either 44.1KHz or 48KHz (can't be sure)
		- Has a bit of aliasing visible near and beyond Nyquist; might be a filter characteristic in combination with the harmonic content it's applied to
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
			kWrap, // Added for easy bipolar ([-1..1]) LFO modulation between kA and kO
			kA,
			kE,
			kI,
			kO,
			kU,
			kNumVowels
		};

	private:
		double m_interpolatedCoeffs[11]; // Calculated by Apply()
		double m_blendCoeff;             // Calculated by Reset()
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
		}

		void Reset();
		
		// Interpolates between vowels ('vowel' range is [0..kNumVowels-1])
		// Don't call more often per second than GetSampleRate()
		void Apply(float &sampleL, float &sampleR, float vowel, float preGain = 0.707f /* -3dB */);

		unsigned GetSampleRate() const
		{
			// It's my estimation that the coefficients were designed for 44.1KHz or 48KHz, so to be safe, I'll pick the latter
			return 48000;;
		}
	};
}
