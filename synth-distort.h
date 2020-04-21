
/*
	FM. BISON hybrid FM synthesis -- Clip & distortion functions.
	(C) 2018- visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

namespace SFM
{
	// Soft clipping without using tanh()
	// Source: https://www.kvraudio.com/forum/viewtopic.php?t=410794
	SFM_INLINE float PolyClip(float sample)
	{
		SFM_ASSERT(sample >= -2.f && sample <= 2.f); // [-2..2] yields a normalized sample, anything outside goes towards infinity
		return 1.5f*sample - 0.5f*sample*sample*sample;
	}

	// Suggested by Thorsten Ørts
	SFM_INLINE float ThorstenClip(float sample)
	{
		// This function won't venture into infinity, whereas SoftClipPoly() alone does
		// It does however *not* stay within normalized sample bounds
		sample = BhaskaraSinf(PolyClip(sample));		
		return sample;
	}

	// Typical soft clip by Nigel Redmon
	SFM_INLINE static float NigelClip(float sample)
	{
		if (sample > 1.f)
			sample = 1.f;
		else if (sample < -1.f)
			sample = -1.f;
		else
			sample = sample * (2.f-fabsf(sample));

		return sample;
	}

	// Udo Zölzer's soft clip (https://github.com/johannesmenzel/SRPlugins/wiki/DSP-ALGORITHMS-Saturation)
	// I'm more comfortable with double precision when using exp() et cetera
	SFM_INLINE static float ZoelzerClip(double sample)
	{
		if (sample > 0.0)
			sample =  1.0 - exp(-sample);
		else
			sample = -1.0 + exp(sample);

		return float(sample);
	}

	// Cubic distortion
	SFM_INLINE float CubicClip(float sample, float amount)
	{	
		SFM_ASSERT(amount >= 0.f && amount <= 1.f);
		return sample - amount*(1.f/3.f)*powf(sample, 3.f);
	}

	// Squarepusher distortion
	SFM_INLINE static float Squarepusher(float sample, float amount)
	{
		const float scaledAmt = 1.f + amount*31.f;
		return atanf(sample*scaledAmt)*(2.f/kPI); // FIXME: try fast_atanf() if this one shows up in performance measurements too much
	}
}
