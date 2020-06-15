
/*
	FM. BISON hybrid FM synthesis -- Hyperbolic tangent & arctangent approximationd (use with care!)
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
	
	Something I spotted on KVR called the 'Pade' method for tan():

	const double pade_tan()
	{
		const double A = 5.0*(-21.0*x+2.0*pow(x, 3.0));
		const double B = 105.0-45.0*pow(x, 2.0)+pow(x, 4.0);
		return A/B;
	}
*/

#pragma once

#include "../synth-global.h"

namespace SFM
{
	// Source: http://www-labs.iro.umontreal.ca/~mignotte/IFT2425/Documents/EfficientApproximationArctgFunction.pdf
	// Domain is strictly [-1..1], outside of that all bets are off
	SFM_INLINE static float fast_atanf(float x)
	{
		const float absX = fabsf(x);
//		return (kPI/4.f)*x - x*(absX-1.f)*(0.2447f + 0.0663f*absX);
		return (kPI/2.f)*x - x*(absX-1.f)*(0.02447f + 0.0663f*absX); // I prefer this curve (plotted in Desmos)
	}

	/*
		Fast approx. hyperbolic tangent functions
		
		Important: 
			- The precision is *not* great so don't use it for trig. that requires proper precision
			- I've graphed ultra_tanhf() and it's pretty accurate, also with respect to the sign, but it's scale seems off,
			  with a little bit of fudging I found multiplying the result by 5.5/PI yields something that's very close to tanh()
	
		Taken from: https://www.kvraudio.com/forum/viewtopic.php?f=33&t=388650&sid=84cf3f5e70cec61f4b19c5031fe3a2d5
	*/

	SFM_INLINE static float fast_tanhf(float x) 
	{
		const float ax = fabsf(x);
		const float x2 = x*x;
		const float z = x * (1.f + ax + (1.05622909486427f + 0.215166815390934f*x2*ax)*x2);
		return z/(1.02718982441289f + fabsf(z));
	}

	// Double precision version
	SFM_INLINE static double fast_tanh(double x) 
	{
		const double ax = fabs(x);
		const double x2 = x*x;
		const double z = x * (1.0 + ax + (1.05622909486427 + 0.215166815390934*x2*ax)*x2);
		return z/(1.02718982441289 + fabs(z));
	}

	// "Ultra" version (claimed to be 2.5 times as precise)
	// Taken from: https://www.kvraudio.com/forum/viewtopic.php?f=33&t=388650&start=15
	SFM_INLINE static float ultra_tanhf(float x)
	{
		const float ax = fabsf(x);
		const float x2 = x*x;
		const float z = x * (0.773062670268356f + ax + (0.757118539838817f + 0.0139332362248817f*x2*x2) * x2*ax);
		return z/(0.795956503022967f + fabsf(z));
	}

	// Double precision version
	SFM_INLINE static double ultra_tanh(double x)
	{
		const double ax = fabs(x);
		const double x2 = x*x;
		const double z = x * (0.773062670268356 + ax + (0.757118539838817 + 0.0139332362248817*x2*x2) * x2*ax);
		return z/(0.795956503022967 + fabs(z));
	}
}
