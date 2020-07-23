
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
	class PinkNoise
	{
	public:
		PinkNoise()
		{
			for (auto &value : m_pinkCoeffs)
				value = 0.0;
		}

		SFM_INLINE float Sample()
		{
			const float whiteNoise = mt_randfc();

			// Added an extra zero to each last constant like Kellet suggested
			m_pinkCoeffs[0] = 0.99886f * m_pinkCoeffs[0] + whiteNoise*0.00555179f;
			m_pinkCoeffs[1] = 0.99332f * m_pinkCoeffs[1] + whiteNoise*0.00750759f;
			m_pinkCoeffs[2] = 0.96900f * m_pinkCoeffs[2] + whiteNoise*0.01538520f;
			m_pinkCoeffs[3] = 0.86650f * m_pinkCoeffs[3] + whiteNoise*0.03104856f;
			m_pinkCoeffs[4] = 0.55000f * m_pinkCoeffs[4] + whiteNoise*0.05329522f;
			m_pinkCoeffs[5] = -0.7616f * m_pinkCoeffs[5] - whiteNoise*0.00168980f;
		
			const float pink = m_pinkCoeffs[0] + m_pinkCoeffs[1] + m_pinkCoeffs[2] + m_pinkCoeffs[3] + m_pinkCoeffs[4] + m_pinkCoeffs[5] + m_pinkCoeffs[6] + whiteNoise*0.5362f;

			m_pinkCoeffs[6] = whiteNoise * 0.115926f;
			
			return pink;
		}
		
	private:
		float m_pinkCoeffs[7];
	};
}
