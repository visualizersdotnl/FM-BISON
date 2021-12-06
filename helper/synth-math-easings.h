
/*
	FM. BISON hybrid FM synthesis -- Interpolation functions all copied from: https://easings.net/ (https://github.com/ai/easings.net/blob/master/src/easings.yml)
	(C) njdewit technologies (visualizers.nl) & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	- All geared to work with single prec. (some functions also available in double prec.)
	- Output domain is [0..1] unless commented it ain't so (scroll down!)
	- They rely on constant 'kPI' (single prec.) declared in /helper/synth-math.h

	FIXME:
		- No input range assertions
*/

#pragma once

#include "../synth-global.h"

namespace SFM
{
	SFM_INLINE static float easeInSinef(float x)
	{
		return 1.f - cosf((x*kPI)*0.5f);
	}

	SFM_INLINE static float easeOutSinef(float x)
	{
		return sinf((x*kPI)*0.5f);
	}

	SFM_INLINE static float easeInOutSinef(float x)
	{
		return -(cosf(kPI*x)-1.f)*0.5f;
	}

	SFM_INLINE static float easeInQuadf(float x)
	{
		return x*x;
	}

	SFM_INLINE static float easeOutQuadf(float x)
	{
		return 1.f - (1.f-x)*(1.f-x);
	}

	SFM_INLINE static float easeInOutQuadf(float x)
	{
		return (x < 0.5f) ? 2.f * x*x : 1.f - powf(-2.f*x + 2.f, 2.f)/2.f;
	}

	SFM_INLINE static float easeInCubicf(float x)
	{
		return x*x*x;
	}

	SFM_INLINE static float easeOutCubicf(float x)
	{
		return 1.f - powf(1.f-x, 3.f);
	}

	SFM_INLINE static float easeInOutCubicf(float x)
	{
		return (x < 0.5f) ? 4.f*x*x*x : 1.f - powf(-2.f*x + 2.f, 3.f)*0.5f;
	}

	SFM_INLINE static float easeInQuartf(float x)
	{
		return x*x*x*x;
	}

	SFM_INLINE static float easeOutQuartf(float x)
	{
		return 1.f - powf(1.f-x, 4.f);
	}

	SFM_INLINE static float easeInOutQuartf(float x)
	{
		return (x < 0.5f) ? 8.f*x*x*x*x : 1.f - powf(-2.f*x + 2.f, 4.f)*0.5f;
	}

	SFM_INLINE static float easeInQuintf(float x)
	{
		return x*x*x*x*x;
	}

	SFM_INLINE static float easeOutQuintf(float x)
	{
		return 1.f - powf(1.f-x, 5.f);
	}

	SFM_INLINE static float easeInOutQuintf(float x)
	{
		return (x < 0.5f) ? 16.f*x*x*x*x*x : 1.f - powf(-2.f*x + 2.f, 5.f)*0.5f;
	}

	SFM_INLINE static float easeInExpof(float x)
	{
		return (0.f != x) ? powf(2.f, 10.f*x - 10.f) : 0.f;
	}

	SFM_INLINE static float easeOutExpof(float x)
	{
		return (1.f != x) ? 1.f - powf(2.f, -10.f*x) : 1.f;
	}

	SFM_INLINE static float easeInOutExpof(float x)
	{
		return (0.f == x)
			? 0.f
			: (1.f == x)
				? 1.f
				: (x < 0.5f) ? powf(2.f, 20.f*x - 10.f)*0.5f
			: (2.f - powf(2.f, -20.f*x + 10.f))*0.5f;
	}

	SFM_INLINE static double easeInCirc(double x)
	{
		return 1.0 - sqrt(1.0 - pow(x, 2.0));
	}

	SFM_INLINE static float easeInCircf(float x)
	{
		return 1.f - sqrtf(1.f - powf(x, 2.f));
	}

	SFM_INLINE static float easeOutCircf(float x)
	{
		return sqrtf(1.f - powf(x-1.f, 2.f));
	}

	SFM_INLINE static float easeInOutCircf(float x)
	{
		return (x < 0.5f)
			? (1.f - sqrtf(1.f - powf(2.f*x, 2.f)))*0.5f
			: (sqrtf(1.f - powf(-2.f*x + 2.f, 2.f)) + 1.f)*0.5f;
	}

	/*
		All functions below go beyond [0..1]
	*/

	SFM_INLINE static float easeInBackf(float x)
	{
		constexpr float c1 = 1.70158f;
		constexpr float c3 = c1 + 1.f;
		return c3 * x*x*x - c1*x*x;
	}

	SFM_INLINE static float easeOutBackf(float x)
	{
		constexpr float c1 = 1.70158f;
		constexpr float c3 = c1 + 1.f;
		return 1.f + c3 * powf(x-1.f, 3.f) + c1 * powf(x-1.f, 2.f);
	}

	SFM_INLINE static float easeInOutBackf(float x)
	{
		constexpr float c1 = 1.70158f;
		constexpr float c2 = c1*1.525f;
		
		const float result = (x < 0.5f)
			? (powf(2.f*x, 2.f) * ((c2+1.f)*2.f * x-c2))*0.5f
			: (powf(2.f*x - 2.f, 2.f) * ((c2+1.f) * (x*2.f - 2.f) + c2) + 2.f)*0.5f;

		return result;
	}

	SFM_INLINE static float easeInElasticf(float x)
	{
		constexpr float c4 = (2.f*kPI)/3.f;
		
		const float result = (0.f == x)
			? 0.f
			: (1.f == x)
				? 1.f
				: -powf(2.f, 10.f*x - 10.f) * sinf((x*10.f - 10.75f) * c4);
		
		return result;
	}

	SFM_INLINE static float easeOutElasticf(float x)
	{
		constexpr float c4 = (2.f*kPI)/3.f;
		
		const float result = (0.f == x)
			? 0.f
			: (1.f == x)
				? 1.f
				: powf(2.f, -10.f*x) * sinf((x*10.f - 0.75f) * c4) + 1.f;

		return result;
	}

	SFM_INLINE static float easeInOutElasticf(float x)
	{
		constexpr float c5 = (2.f*kPI)/4.5f;

		const float result = (0.f == x)
			? 0.f
			: (1.f == x)
				? 1.f
				: (x < 0.5f)
					? -(powf(2.f, 20.f*x - 10.f) * sinf((20.f*x - 11.125f) * c5))*0.5f
					: (powf(2.f, -20.f*x + 10.f) * sinf((20.f*x - 11.125f) * c5))*0.5f + 1.f;

		return result;
	}

	SFM_INLINE static float easeOutBouncef(float x)
	{
		constexpr float n1 = 7.5625f;
		constexpr float d1 = 2.75f;

		float result;
		if (x < 1.f/d1)
		{
			result = n1 * x*x;
		}
		else if (x < 2.f/d1)
		{
			x -= 1.5f;
			result = n1 * (x/d1)*x + 0.75f;
		}
		else if (x < 2.5f/d1)
		{
			x -= 2.25f;
			result = n1 * (x/d1)*x + 0.9375f;
		}
		else
		{
			x -= 2.625f;
			result = n1 * (x/d1)*x + 0.984375f;
		}

		return result;
	}

	SFM_INLINE static float easeInBouncef(float x)
	{
		return 1.f - easeOutBouncef(1.f-x);
	}

	SFM_INLINE static float easeInOutBouncef(float x)
	{
		return (x < 0.5f)
			? (1.f - easeOutBouncef(1.f - 2.f*x))*0.5f
			: (1.f + easeOutBouncef(2.f*x + 1.f))*0.5f;
	}
}
