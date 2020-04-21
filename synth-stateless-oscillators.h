
/*
	FM. BISON hybrid FM synthesis -- Stateless oscillator functions.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Phase is [0..1]
	- Anti-aliased oscillators are called 'oscPoly...'
*/

#pragma once

namespace SFM
{
	/*
		Sine/Cosine
	*/

	SFM_INLINE static float oscSine(float phase) 
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return fast_sinf(phase); 
	}
	
	SFM_INLINE static float oscCos(float phase) 
	{ 
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return fast_cosf(phase); 
	}

	/*
		Ramp, sawtooth, square, triangle & pulse (will alias at higher rates).
	*/

	SFM_INLINE static float oscRamp(float phase)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return phase*2.f - 1.f;
	}

	SFM_INLINE static float oscSaw(float phase)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return phase*-2.f + 1.f;
	}

	SFM_INLINE static float oscSquare(float phase)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return phase >= 0.5f ? 0.f : -1.f;
	}

	SFM_INLINE static float oscTriangle(float phase)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return 2.f * (fabsf(-1.f + (2.f*phase))-0.5f);
	}

	SFM_INLINE static float oscPulse(float phase, float duty)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		const float cycle = phase;
		return (cycle < duty) ? 1.f : -1.f;
	}

	/*
		Anti-aliased oscillators (PolyBLEP)
		
		Source: 
		- http://www.kvraudio.com/ (lost exact link!)
		- Martin Finke (http://www.martin-finke.de/blog/articles/audio-plugins-018-polyblep-oscillator/)
	*/

	SFM_INLINE static float BiPolyBLEP(float t, float w)
	{
		// Not near point?
		if (fabsf(t) >= w)
			return 0.f;

		// Near point: smoothen
		t /= w;
		float tt1 = t*t + 1.f;
		if (t >= 0.f)
			tt1 = -tt1;
		
		return tt1 + t+t;
	}

	SFM_INLINE static float oscPolySaw(float phase, float width /* Generally 'frequency/(sampleRate/width)' */)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		const float closestUp = float(phase-0.5f >= 0.f);
		
		float saw = oscSaw(phase);
		saw += BiPolyBLEP(phase - closestUp, width);

		return saw;
	}

	SFM_INLINE static float oscPolyPulse(float phase, float width, float duty)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		const float closestUp   = float(phase-0.5f >= 0.f);
		const float closestDown = float(phase-0.5f >= duty) - float(phase+0.5f < duty) + duty;
		
		float pulse = oscPulse(phase, duty);

		pulse += BiPolyBLEP(phase - closestUp,   width);
		pulse -= BiPolyBLEP(phase - closestDown, width);

		return pulse;
	}

	/*
		White noise
	*/

	SFM_INLINE static float oscWhiteNoise()
	{
		return Clamp(-1.f + mt_randf()*2.f);
	}

	/*
		Pink noise (-3dB LPF)

		Source: http://www.firstpr.com.au/dsp/pink-noise/ (using Paul Kellet's refined method)
	*/

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
		
		// FIXME: at times it over- or undershoots
		return Clamp(pink);
	}
}
