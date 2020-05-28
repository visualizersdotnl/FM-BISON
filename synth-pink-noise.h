
/*
	FM. BISON hybrid FM synthesis -- Pink noise oscillator.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Source: http://www.firstpr.com.au/dsp/pink-noise/ (using Paul Kellet's refined method)
*/

#pragma once

#include "synth-global.h"
#include "synth-stateless-oscillators.h"

namespace SFM
{
	class PinkNoiseState
	{
	public:
		PinkNoiseState()
		{
			for (auto &value : m_pinkCoeffs)
				value = oscWhiteNoise();
		}
		
		float m_pinkCoeffs[7];
	};

	SFM_INLINE static float oscPinkNoise(PinkNoiseState &state)
	{
		const float whiteNoise = oscWhiteNoise();

		// Added an extra zero to each last constant like Kellet suggested
		state.m_pinkCoeffs[0] = 0.99886f * state.m_pinkCoeffs[0] + whiteNoise*0.00555179f;
		state.m_pinkCoeffs[1] = 0.99332f * state.m_pinkCoeffs[1] + whiteNoise*0.00750759f;
		state.m_pinkCoeffs[2] = 0.96900f * state.m_pinkCoeffs[2] + whiteNoise*0.01538520f;
		state.m_pinkCoeffs[3] = 0.86650f * state.m_pinkCoeffs[3] + whiteNoise*0.03104856f;
		state.m_pinkCoeffs[4] = 0.55000f * state.m_pinkCoeffs[4] + whiteNoise*0.05329522f;
		state.m_pinkCoeffs[5] = -0.7616f * state.m_pinkCoeffs[5] - whiteNoise*0.00168980f;
		
		// This can be optimized by adding these coefficients as you go (FIXME)
		const float pink = state.m_pinkCoeffs[0] + state.m_pinkCoeffs[1] + state.m_pinkCoeffs[2] + state.m_pinkCoeffs[3] + state.m_pinkCoeffs[4] + state.m_pinkCoeffs[5] + state.m_pinkCoeffs[6] + whiteNoise*0.5362f;

		state.m_pinkCoeffs[6] = whiteNoise * 0.115926f;
		
		// FIXME: at times it over- or undershoots, try double precision?
		return Clamp(pink);
	}
}
