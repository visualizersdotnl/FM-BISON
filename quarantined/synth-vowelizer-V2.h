
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	We must get rid of this in favour of an actual formant filter as soon as possible!
    
	The actual idea of vocoding is feeding a reference or analysis signal through a series of bandpasses and in turn bandpassing
	a harmonically rich signal (such as white noise or a saw wave) through the same set of bandpasses, amplified by the output
	of the analysis pass. I know this, now to find a way to bend this into something that can make a random (collection of) notes
	says "AAAH".

	Here's a nice video on the subject matter (haven't watched it in full yet): https://youtu.be/nPFzhwAJhGI
*/

#pragma once

#include "../3rdparty/SvfLinearTrapOptimised2.hpp"

#include "../synth-global.h"

namespace SFM
{
	class VowelizerV2
	{
	public:
		enum Vowel
		{
			kEE,
			kOO,
			kI,
			kE,
			kU,
			kA,
			kNumVowels
		};

	public:
		VowelizerV2(unsigned sampleRate) :
			m_sampleRate(sampleRate)
		{
			Reset();
		}

		SFM_INLINE void Reset()
		{
			for (auto &filter : m_filterBP)
				filter.resetState();
		}

		void Apply(float &left, float &right, Vowel vowel);

	private:
		const unsigned m_sampleRate;

		SvfLinearTrapOptimised2 m_preFilter;
		SvfLinearTrapOptimised2 m_filterBP[3];		
	};
}