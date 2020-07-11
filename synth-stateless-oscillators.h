
/*
	FM. BISON hybrid FM synthesis -- Stateless oscillator functions.
	(C) visualizers.nl & bipolaraudio.nl
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

	SFM_INLINE static float oscSine(double phase) 
	{
		return fast_sinf(float(phase)); 
	}
	
	SFM_INLINE static float oscCos(double phase) 
	{ 
		return fast_cosf(float(phase)); 
	}

	/*	Naive implementations (not band-limited) */

	SFM_INLINE static float oscSaw(double phase)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		phase += 0.5f;
		return float(2.f*phase - 1.f);
	}

	SFM_INLINE static float oscRamp(double phase)
	{
		return -1.f*oscSaw(phase);
	}

	SFM_INLINE static float oscSquare(double phase)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		return phase >= 0.5 ? 1.f : -1.f;
	}

	SFM_INLINE static float oscTriangle(double phase)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		return float(-2.0 * (fabs(-1.0 + (2.0*phase))-0.5));
	}

	SFM_INLINE static float oscPulse(double phase, float duty)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		SFM_ASSERT(duty >= 0.f && duty <= 1.f);

		const float cycle = float(phase);
		return (cycle < duty) ? 1.f : -1.f;
	}

	SFM_INLINE static float oscBox(double phase)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		return phase >= 0.25 && phase <= 0.75 ? 1.f : -1.f;
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

		template<typename T> SFM_INLINE static int64_t bitwiseOrZero(const T &value) 
		{
			return static_cast<int64_t>(value) | 0;
		}

		// Adapted from "Phaseshaping Oscillator Algorithms for Musical Sound Synthesis" by Jari Kleimola, Victor Lazzarini, Joseph Timoney, and Vesa Valimaki.
		// http://www.acoustics.hut.fi/publications/papers/smc2010-phaseshaping/
		SFM_INLINE static double BLEP(double point, double dT /* This is, usually, just the pitch of the oscillator */) 
		{
			if (point < dT)
				// Discontinuities between 0 & 1
				return -Squared(point/dT - 1.0);
			else if (point > 1.0 - dT)
				// Discontinuities between -1 & 0
				return Squared((point - 1.0)/dT + 1.0);
			else
				return 0.0;
		}
	
		// By Tale (KVR): http://www.kvraudio.com/forum/viewtopic.php?t=375517
		SFM_INLINE static double BLEP_by_Tale(double point, double dT) 
		{
			if (point < dT)
			{
				// Discontinuities between 0 & 1
				point /= dT;
				return point+point - point*point - 1.0;
			}			
			else if (point > 1.0 - dT)
			{
				// Discontinuities between -1 & 0
				point = (point - 1.0)/dT;
				return point*point + point+point + 1.0;
			}
			else
				return (float) 0.0;
		}

		SFM_INLINE static double BLAMP(double point, double dT)
		{
			if (point < dT) 
			{
				point = point/dT - 1.0;
				return -1.0 / 3.0*Squared(point) * point;
			} 
			else if (point > 1.0 - dT) 
			{
				point = (point - 1.0) /dT + 1.0;
				return 1.0 / 3.0*Squared(point) * point;
			} 
			else 
				return 0.0;
		}
	}

	SFM_INLINE static float oscPolySquare(double phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		SFM_ASSERT(pitch > 0.0);

		double P1 = phase + 0.5;
		P1 -= Poly::bitwiseOrZero(P1);

		double square = phase < 0.5 ? 1.0 : -1.0;
		square += Poly::BLEP(phase, pitch) - Poly::BLEP(P1, pitch);

		return float(square);
	}

	SFM_INLINE static float oscPolySaw(double phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		SFM_ASSERT(pitch > 0.0);

		double P1 = phase + 0.5;
		P1 -= Poly::bitwiseOrZero(P1);

		double saw = 2.0*P1 - 1.0;
		saw -= Poly::BLEP(P1, pitch);

		return float(saw);
	}

	SFM_INLINE static float oscPolyRamp(double phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		SFM_ASSERT(pitch > 0.0);

		double P1 = phase;
		P1 -= Poly::bitwiseOrZero(P1);

		double ramp = 1.0 - 2.0*P1;
		ramp += Poly::BLEP(P1, pitch);

		return float(ramp);
	}

	SFM_INLINE static float oscPolyTriangle(double phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		SFM_ASSERT(pitch > 0.0);

		double P1 = phase + 0.25;
		double P2 = phase + 0.75;
		P1 -= Poly::bitwiseOrZero(P1);
		P2 -= Poly::bitwiseOrZero(P2);

		double triangle = phase*4.0;
		if (triangle >= 3.0)
		{
			triangle -= 4.0;
		}
		else if (triangle > 1.0)
		{
			triangle = 2.0 - triangle;
		}

		triangle += 4.0 * pitch * (Poly::BLAMP(P1, pitch) - Poly::BLAMP(P2, pitch));

		return float(triangle);
	}

	SFM_INLINE static float oscPolyRectifiedSine(double phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		SFM_ASSERT(pitch > 0.0);

		double P1 = phase + 0.25;
		P1 -= Poly::bitwiseOrZero(P1);

		double rectified = 2.0 * oscSine(0.5 * P1) - 4.0*0.5f;
		rectified += 2.0 * pitch * Poly::BLAMP(P1, pitch);

		return float(rectified) * 0.707f; // Curtail a bit (-3dB), otherwise it's just too loud
	}

	SFM_INLINE static float oscPolyRectangle(double phase, double pitch, float width)
	{
		SFM_ASSERT(phase >= 0.0 && phase <= 1.0);
		SFM_ASSERT(pitch > 0.0);
		SFM_ASSERT(width > 0.f && width <= 1.f);

		double P1 = phase + 1.0-width;
		P1 -= Poly::bitwiseOrZero(P1);

		double rectangle = -2.0*width;
		if (phase < width)
			rectangle += 2.0;

		rectangle += Poly::BLEP(phase, pitch) - Poly::BLEP(P1, pitch);

		return float(rectangle);
	}

	/*
		White noise
	*/

	SFM_INLINE static float oscWhiteNoise()
	{
		return mt_randfc();
	}
}
