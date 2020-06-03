
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

	SFM_INLINE static float oscSine(float phase) 
	{
		return fast_sinf(phase); 
	}
	
	SFM_INLINE static float oscCos(float phase) 
	{ 
		return fast_cosf(phase); 
	}

	/*
		Naive implementations (not band-limited)
	*/

	// There are conflicting opinions on how a ramp or a saw looks
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

	// I am not sure where this *originally* came from, but I took it from Sander Vermeer's synth.
	// Desmos link: https://www.desmos.com/calculator/l6cc64mqhk
	SFM_INLINE static float oscAltSaw(float phase, float frequency)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		auto residual = [](float phase, float frequency) 
		{
			// Strength of distortion is based on frequency; higher value means less strength
			const float strength = 50.f/frequency;
			const float strengthSq = strength*strength*strength*strength;
			const float denominator = (strengthSq * 100.f * phase*phase*phase*phase*phase*phase) + 1.f;
			return 1.f/denominator;
		};

		const float phaseMinOne = phase-1.f;
		float saw = 2.f*phaseMinOne*phaseMinOne - 1.f;
		saw *= residual(phase, frequency);

		return saw;
	}

	/*
		Band-limited (PolyBLEP) oscillators
		
		- Based on: https://github.com/martinfinke/PolyBLEP
		- A copy of the original can be found @ /3rdparty/PolyBlep/...
		- There are a lot more waveforms ready to use (though I shouldn't go overboard considering FM)
		
		I've kept this implementation and it's helper functions all in one spot, though I did
		rename a few things to keep it consistent with my style.
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
		SFM_INLINE static float BLEP_Original(double point, double dT /* This is, usually, just the pitch of the oscillator */) 
		{
			if (point < dT)
				// Discontinuities between 0 & 1
				return (float) -Squared(point/dT - 1.0);
			else if (point > 1.0 - dT)
				// Discontinuities between -1 & 0
				return (float) Squared((point - 1.0)/dT + 1.0);
			else
				return 0.f;
		}
	
		// Source: http://metafunction.co.uk/all-about-digital-oscillators-part-2-blits-bleps/
		SFM_INLINE static float BLEP(double point, double dT /* This is, usually, just the pitch of the oscillator */) 
		{
			if (point < dT)
			{
				// Discontinuities between 0 & 1
				point /= dT;
				return float(point+point - point*point - 1.0);
			}			
			else if (point > 1.0 - dT)
			{
				// Discontinuities between -1 & 0
				point = (point - 1.0)/dT;
				return float(point*point + point+point + 1.0);
			}
			else
				return 0.f;
		}

		SFM_INLINE static float BLAMP(double point, double dT)
		{
			if (point < dT) 
			{
				point = point / dT - 1.0;
				return float(-1.0 / 3.0 * Squared(point) * point);
			} 
			else if (point > 1.0 - dT) 
			{
				point = (point - 1.0) / dT + 1.0;
				return float(1.0 / 3.0 * Squared(point) * point);
			} 
			else 
				return 0.f;
		}
	}

	SFM_INLINE static float oscPolySquare(float phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		float P1 = phase + 0.5f;
		P1 -= Poly::bitwiseOrZero(P1);

		float square = phase < 0.5f ? 1.f : -1.f;
		square += Poly::BLEP(phase, pitch) - Poly::BLEP(P1, pitch);

		return square;
	}

	SFM_INLINE static float oscPolySaw(float phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		float P1 = phase + 0.5f;
		P1 -= Poly::bitwiseOrZero(P1);

		float saw = 2.f*P1 - 1.f;
		saw -= Poly::BLEP(P1, pitch);

		return saw;
	}

	SFM_INLINE static float oscPolyRamp(float phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		float P1 = phase;
		P1 -= Poly::bitwiseOrZero(P1);

		float ramp = 1.f - 2.f*P1;
		ramp -= Poly::BLEP(P1, pitch);

		return ramp;
	}

	SFM_INLINE static float oscPolyTriangle(float phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

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

		triangle += 4.f * float(pitch) * (Poly::BLAMP(P1, pitch) - Poly::BLAMP(P2, pitch));

		return triangle;
	}

	SFM_INLINE static float oscPolyRectifiedSine(float phase, double pitch)
	{
		SFM_ASSERT(phase >= 0.f && phase <= 1.f);

		float P1 = phase + 0.25f;
		P1 -= Poly::bitwiseOrZero(P1);

//		float rectified = 2.f * sinf(kPI * P1) - 4.f/kPI;
//		rectified += k2PI * pitch * Poly::BLAMP(P1, pitch);

		float rectified = 2.f * oscSine(0.5f * P1) - 4.f*0.5f;
		rectified += 2.f * float(pitch) * Poly::BLAMP(P1, pitch);

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
