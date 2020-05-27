
/*
	FM. BISON hybrid FM synthesis -- Misc. math functions & constants.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

namespace SFM
{
	// Constants (taken from visualizers.nl's Std3DMath library)
	constexpr float kPI = 3.1415926535897932384626433832795f;
	constexpr float kHalfPI = kPI*0.5f;
	constexpr float k2PI = 2.f*kPI;
	constexpr float kEpsilon = 5.96e-08f; // Max. error for single precision (32-bit).
	constexpr float kGoldenRatio = 1.61803398875f;
	constexpr float kRootHalf = 0.70710678118f;

	// Bezier smoothstep
	SFM_INLINE static float smoothstepf(float t)
	{
		SFM_ASSERT(t >= 0.f && t <= 1.f);
		return t*t * (3.f - 2.f*t);
	}

	// Inverse square
	SFM_INLINE static float invsqrf(float x)
	{
		x = 1.f-x;
		return 1.f - x*x;
	}

	// Scalar interpolation
	template<typename T>
	SFM_INLINE static const T lerpf(const T &a, const T &b, float t)
	{
		return a + (b-a)*t;
	}

	// (GLSL) frac()
	SFM_INLINE static float fracf(float value) 
	{ 
		return fabsf(value - int(value)); 
	}

	// (HLSL) saturate
	SFM_INLINE float saturatef(float value)
	{
		if (value < 0.f)
			return 0.f;

		else if (value > 1.f)
			return 1.f;

		return value;
	}
	
	// Approx. sinf() derived from mr. Bhaskara's theory
	SFM_INLINE float BhaskaraSinf(float x)
	{
		return 16.f * x * (kPI-x) / (5.f * kPI*kPI - 4.f * x * (kPI-x));
	}

	// Ham-fisted fast floating point modulo which only works fairly well for smaller values
	// https://stackoverflow.com/questions/9505513/floating-point-modulo-operation/37977740#comment12038890_9505761
	SFM_INLINE static float fast_fmodf(float value, float modulo)
	{
		return value - truncf(value/modulo)*modulo;
	}

	SFM_INLINE static float fast_fmodf_one(float value)
	{
		return value - truncf(value);
	}
}

#include "synth-fast-cosine.h"
#include "synth-fast-tan.h"

