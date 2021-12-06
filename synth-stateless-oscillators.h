
/*
	FM. BISON hybrid FM synthesis -- Stateless oscillator functions.
	(C) njdewit technologies (visualizers.nl) & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- Phase is [0..1], this range must be adhered to except for oscSine() and oscCos()
	- Band-limited (PolyBLEP) oscillators are called 'oscPoly...'
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	/*
		Sin/Cos
	*/

	SFM_INLINE static float oscSine(float phase) 
	{
		return fast_sinf(phase); 
	}
	
	SFM_INLINE static float oscCos(float phase) 
	{ 
		return fast_cosf(phase); 
	}

	/* Naive implementations (not band-limited) */

	SFM_INLINE static float oscSaw(float phase)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		phase += 0.5f;
		return 2.f*phase - 1.f;
	}

	SFM_INLINE static float oscRamp(float phase)
	{
		return -1.f*oscSaw(phase);
	}

	SFM_INLINE static float oscSquare(float phase)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return phase >= 0.5 ? 1.f : -1.f;
	}

	SFM_INLINE static float oscTriangle(float phase)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return -2.f * (fabsf(-1.f + (2.f*phase))-0.5f);
	}

	SFM_INLINE static float oscPulse(float phase, float duty)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		SFM_ASSERT(duty >= 0.f && duty <= 1.f);

		const float cycle = phase;
		return (cycle < duty) ? 1.f : -1.f;
	}

	SFM_INLINE static float oscBox(float phase)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		return phase >= 0.25f && phase <= 0.75f ? 1.f : -1.f;
	}

	/*
		Band-limited (PolyBLEP) oscillators
		
		- Links:
		  * https://github.com/martinfinke/PolyBLEP 
		  * http://www.kvraudio.com/forum/viewtopic.php?t=375517
		  * https://dsp.stackexchange.com/questions/54790/polyblamp-anti-aliasing-in-c
		
		- A copy of Martin Finke's implementation can be found @ /3rdparty/PolyBLEP
		
		I've kept this implementation and it's helper functions all in one spot, though I did
		rename a few things to keep it consistent with my style

		Keep in mind that PolyBLEP is *not* the best solution for band-limited oscillators, but it does a relatively good job for
		it's CPU footprint; it gets better the higher the sample rate is
	*/

	namespace Poly
	{
		template<typename T> SFM_INLINE static T Squared(const T &value)
		{
			return value*value;
		}

		SFM_INLINE static int32_t bitwiseOrZero(float value) 
		{
			return static_cast<int32_t>(value) | 0;
		}

		// Adapted from "Phaseshaping Oscillator Algorithms for Musical Sound Synthesis" by Jari Kleimola, Victor Lazzarini, Joseph Timoney, and Vesa Valimaki.
		// http://www.acoustics.hut.fi/publications/papers/smc2010-phaseshaping/
		SFM_INLINE static float BLEP(float point, float dT /* This is, usually, just the pitch of the oscillator */) 
		{
			if (point < dT)
				// Discontinuities between 0 & 1
				return -Squared(point/dT - 1.f);
			else if (point > 1.f - dT)
				// Discontinuities between -1 & 0
				return Squared((point - 1.f)/dT + 1.f);
			else
				return 0.f;
		}
	
		// By Tale: http://www.kvraudio.com/forum/viewtopic.php?t=375517
		SFM_INLINE static float BLEP_by_Tale(float point, float dT) 
		{
			if (point < dT)
			{
				// Discontinuities between 0 & 1
				point /= dT;
				return point+point - point*point - 1.f;
			}			
			else if (point > 1.f-dT)
			{
				// Discontinuities between -1 & 0
				point = (point-1.f)/dT;
				return point*point + point+point + 1.f;
			}
			else
				return 0.f;
		}

		SFM_INLINE static float BLAMP(float point, float dT)
		{
			if (point < dT) 
			{
				point = point/dT - 1.f;
				return -1.f / 3.f*Squared(point) * point;
			} 
			else if (point > 1.f - dT) 
			{
				point = (point - 1.f) /dT + 1.f;
				return 1.f / 3.f*Squared(point) * point;
			} 
			else 
				return 0.f;
		}
	}

	SFM_INLINE static float oscPolySquare(float phase, float pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		SFM_ASSERT(pitch > 0.f);

		float P1 = phase + 0.5f;
		P1 -= Poly::bitwiseOrZero(P1);

		float square = phase < 0.5f ? 1.f : -1.f;
		square += Poly::BLEP(phase, pitch) - Poly::BLEP(P1, pitch);

		return square;
	}

	SFM_INLINE static float oscPolySaw(float phase, float pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		SFM_ASSERT(pitch > 0.f);

		float P1 = phase + 0.5f;
		P1 -= Poly::bitwiseOrZero(P1);

		float saw = 2.f*P1 - 1.f;
		saw -= Poly::BLEP(P1, pitch);

		return saw;
	}

	SFM_INLINE static float oscPolyRamp(float phase, float pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		SFM_ASSERT(pitch > 0.f);

		float P1 = phase;
		P1 -= Poly::bitwiseOrZero(P1);

		float ramp = 1.f - 2.f*P1;
		ramp += Poly::BLEP(P1, pitch);

		return ramp;
	}

	SFM_INLINE static float oscPolyTriangle(float phase, float pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		SFM_ASSERT(pitch > 0.f);

		float P1 = phase + 0.25f;
		float P2 = phase + 0.75f;
		P1 -= Poly::bitwiseOrZero(P1);
		P2 -= Poly::bitwiseOrZero(P2);

		float triangle = phase*4.f;
		if (triangle >= 3.f)
		{
			triangle -= 4.f;
		}
		else if (triangle > 1.f)
		{
			triangle = 2.f - triangle;
		}

		triangle += 4.f * pitch * (Poly::BLAMP(P1, pitch) - Poly::BLAMP(P2, pitch));

		return triangle;
	}

	SFM_INLINE static float oscPolyRectifiedSine(float phase, float pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		SFM_ASSERT(pitch > 0.f);

		float P1 = phase + 0.25f;
		P1 -= Poly::bitwiseOrZero(P1);

		float rectified = 2.f * oscSine(0.5f * P1) - 4.f*0.5f;
		rectified += 2.f * pitch * Poly::BLAMP(P1, pitch);

		return rectified;
	}

	SFM_INLINE static float oscPolyRectangle(float phase, float pitch, float width)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);
		SFM_ASSERT(pitch > 0.f);
		SFM_ASSERT(width > 0.f && width <= 1.f);

		float P1 = phase + 1.f-width;
		P1 -= Poly::bitwiseOrZero(P1);

		float rectangle = -2.f*width;
		if (phase < width)
			rectangle += 2.f;

		rectangle += Poly::BLEP(phase, pitch) - Poly::BLEP(P1, pitch);

		return rectangle;
	}

	/*
		White noise
	*/

	SFM_INLINE static float oscWhiteNoise()
	{
		return mt_randfc();
	}
}
