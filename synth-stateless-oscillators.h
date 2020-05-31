
/*
	FM. BISON hybrid FM synthesis -- Stateless oscillator functions.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Phase is [0..1]
	- Band-limited oscillators are called 'oscPoly...'
*/

#pragma once

#include "synth-global.h"

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
		Ramp, sawtooth, square, triangle & pulse (not band-limited)
	*/

	// Some refer to the ramp being equal to a saw, but to me this way around makes more sense.
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
		return phase >= 0.5f ? 1.f : -1.f;
	}

	SFM_INLINE static float oscTriangle(float phase)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return -2.f * (fabsf(-1.f + (2.f*phase))-0.5f);
	}

	SFM_INLINE static float oscPulse(float phase, float duty)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		const float cycle = phase;
		return (cycle < duty) ? 1.f : -1.f;
	}

	/*
		Band-limited oscillators (using PolyBLEP)

		These functions were lifted from: https://github.com/martinfinke/PolyBLEP

		A copy of the original can be found in /3rdparty/PolyBLEP:
		- There are a lot more waveforms ready to use.
		- I should go over it once more to see if I got everything right (FIXME).

		I've kept this implementation and it's helper functions all in one spot.
		I did rename a few variables and functions for readability.
	*/

	template<typename T> SFM_INLINE static T Squared(const T &value)
	{
		return value*value;
	}

	template<typename T> SFM_INLINE static int64_t bitwiseOrZero(const T &value) 
	{
		return static_cast<int64_t>(value) | 0;
	}

	// Adapted from "Phaseshaping Oscillator Algorithms for Musical Sound Synthesis" by Jari Kleimola, Victor Lazzarini, Joseph Timoney, and Vesa Valimaki.
	// http://www.acoustics.hut.fi/publications/papers/smc2010-phaseshaping/
	SFM_INLINE static float PolyBLEP(double point, double DT) 
	{
		if (point < DT)
			return (float) -Squared(point/DT - 1.0);
		else if (point > (1.0 - DT))
			return (float) Squared((point - 1.0)/DT + 1.0);
		else
			return 0.f;
	}
	
	SFM_INLINE static float PolyBLAMP(double point, double DT)
	{
		if (point < DT) 
		{
			point = point / DT - 1.0;
			return float(-1.0 / 3.0 * Squared(point) * point);
		} 
		else if (point > 1 - DT) 
		{
			point = (point - 1.0) / DT + 1.0;
			return float(1.0 / 3.0 * Squared(point) * point);
		} 
		else 
			return 0.f;
	}

	SFM_INLINE static float oscPolySquare(float phase, float DT)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		float P1 = phase + 0.5f;
		P1 -= bitwiseOrZero(P1);

		float square = phase < 0.5f ? 1.f : -1.f;
		square += PolyBLEP(phase, DT) - PolyBLEP(P1, DT);

		return square;
	}

	SFM_INLINE static float oscPolySaw(float phase, float DT)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		float P1 = phase + 0.5f;
		P1 -= bitwiseOrZero(P1);

		float saw = 2.f*P1 - 1.f;
		saw -= PolyBLEP(P1, DT);

		return saw;
	}

	SFM_INLINE static float oscPolyTriangle(float phase, float DT)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		float P1 = phase + 0.25f;
		float P2 = phase + 0.75f;
		P1 -= bitwiseOrZero(P1);
		P2 -= bitwiseOrZero(P2);

		float triangle = phase*4.f;
		if (triangle >= 3.f)
			triangle -= 4.f;
		else if (triangle > 1.f)
			triangle = 2.f - triangle;

		triangle += 4.f * DT * (PolyBLAMP(P1, DT) - PolyBLAMP(P2, DT));

		return triangle;
	}

	SFM_INLINE static float oscPolyRectifiedSine(float phase, float DT)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		float P1 = phase + 0.25f;
		P1 -= bitwiseOrZero(P1);

//		float rectified = 2.f * sinf(kPI * P1) - 4.f/kPI;
//		rectified += k2PI * DT * PolyBLAMP(P1, DT);

		float rectified = 2.f * oscSine(0.5f * P1) - 4.f*0.5f;
		rectified += 2.f * DT * PolyBLAMP(P1, DT);

		return rectified;
	}

	/*
		White noise
	*/

	SFM_INLINE static float oscWhiteNoise()
	{
		return mt_randfc();
	}
}
