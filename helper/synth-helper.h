
/*
	FM. BISON hybrid FM synthesis -- Helper functions (mostly math related to frequencies, notes et cetera).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

// To modify floating point behaviour (SSE)
#include <xmmintrin.h>
#include <pmmintrin.h>

#include <cmath>
#include <limits>

#include "../synth-global.h"

#include "synth-math.h"

namespace SFM 
{
	// Use this to, within a scope, treat all denormals as zero; this save a lot of headache when working with floating point numbers!
	// Important: SSE (and possibly AVX, check, FIXME) only!
	class DisableDenormals
	{
	public:
		DisableDenormals()
		{
			m_mxcsrRestore = _mm_getcsr();

			// Set SSE FTZ and DAZ flags (https://www.cita.utoronto.ca/~merz/intel_c10b/main_cls/mergedProjects/fpops_cls/common/fpops_set_ftz_daz.htm)
			// Basically we're treating all DEN as zero
			_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
			_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
		}

		~DisableDenormals()
		{
			// Restore SSE flags
			_mm_setcsr(m_mxcsrRestore);
		}

	private:
		unsigned m_mxcsrRestore;		
	};

	/* ----------------------------------------------------------------------------------------------------

		Frequency

	 ------------------------------------------------------------------------------------------------------ */

	// Frequency to pitch
	SFM_INLINE static double CalculatePitch(double frequency, unsigned sampleRate)
	{
		return frequency/sampleRate;
	}

	// Frequency to angular pitch
	SFM_INLINE static double CalculateAngularPitch(double frequency, unsigned sampleRate)
	{
		return CalculatePitch(frequency, sampleRate)*k2PI;
	}

	// Note to frequency
	SFM_INLINE static float NoteToFreq(unsigned note)
	{
		return float(kBaseHz * pow(2.0, (note - 69.0) / 12.0 /* 1 octave equals 12 semitones */));
	}

	/* ----------------------------------------------------------------------------------------------------

		dB/Gain

	 ------------------------------------------------------------------------------------------------------ */

	// To and from dB
	SFM_INLINE static float GainTodB(float amplitude) { return 20.f * log10f(amplitude); }
	SFM_INLINE static float dBToGain(float dB)        { return powf(10.f, dB/20.f);      }

	// The following 2 functions were procured from some code I stumbled across by "ChunkWare Music Software",
	// and I've added them to the arsenal; I haven't done any testing but they're probably a bit less
	// precise than their textbook counterparts above, but perhaps faster

	SFM_INLINE static float Lin2dB(double linear) 
	{
		constexpr double LOG_2_DB = 8.6858896380650365530225783783321;
		return float(log(linear)*LOG_2_DB);
	}
	
	SFM_INLINE static float dB2Lin(double dB) 
	{
		constexpr double DB_2_LOG = 0.11512925464970228420089957273422;
		return float(exp(dB*DB_2_LOG));
	}

	/* ----------------------------------------------------------------------------------------------------

		Misc.

	 ------------------------------------------------------------------------------------------------------ */

	// Integer is power of 2?
	SFM_INLINE static bool IsPow2(unsigned value)
	{
		return value != 0 && !(value & (value - 1));
	}

	// Hard clamp (sample)
	SFM_INLINE static float Clamp(float sample)
	{
		if (sample > 1.f)
			return 1.f;
		
		if (sample < -1.f)
			return -1.f;
		
		return sample;
	}

	// Stereo linking (and rectification)
	SFM_INLINE static float GetRectifiedMaximum(float sampleL, float sampleR)
	{
		return std::max<float>(fabsf(sampleL), fabsf(sampleR));
	}

	/* ----------------------------------------------------------------------------------------------------

		Checks & assertions.

	 ------------------------------------------------------------------------------------------------------ */

	// Floating point error detection
	SFM_INLINE static bool FloatCheck(float value)
	{
		if (value != 0.f && fabsf(value) < std::numeric_limits<float>::denorm_min())
			return false;
			 
		if (true == std::isnan(value))
			return false;

		if (true == std::isinf(value))
			return false;

		return true;
	}
	
	// 'Unreferenced formal parameter'
	#pragma warning (push)
	#pragma warning (disable: 4100)

	SFM_INLINE static void FloatAssert(float value)
	{
		SFM_ASSERT(true == FloatCheck(value));
	}

	// Sample check function (range & floating point).
	SFM_INLINE static void SampleAssert(float sample)
	{
		SFM_ASSERT(true == FloatCheck(sample));	
		SFM_ASSERT(sample >= -1.f && sample <= 1.f);
	}

	#pragma warning (pop)

	/* ----------------------------------------------------------------------------------------------------

		Helper functions for SVF filter.

	 ------------------------------------------------------------------------------------------------------ */

	// Normalized cutoff [0..1] to Hz
	SFM_INLINE static float CutoffToHz(float cutoff, unsigned Nyquist, float minCutoff = kSVFMinFilterCutoffHz)
	{
		// Allowed according to SVF impl.: [16.0..Nyquist]
		const unsigned maxCutoff = Nyquist;
		SFM_ASSERT(cutoff >= 0.f && cutoff <= 1.f);
		return minCutoff + cutoff*(maxCutoff-minCutoff);
	}

	// Normalized resonance [0..1] to Q
	SFM_INLINE static float ResoToQ(float resonance)
	{
		// Allowed: [0.025..40.0]
		// Default: 0.5
		SFM_ASSERT(resonance >= 0.f && resonance <= 1.f);

		const float Q = kSVFMinFilterQ + resonance*kSVFFilterQRange;
		SFM_ASSERT(Q <= 40.f);
		return Q;
	}

	// Snap floating point value to zero (nicked from JUCE)
	SFM_INLINE static float SnapToZero(float value)
	{
		return (false == (value < -1.0e-8f || value > 1.0e-8f)) ? 0.f : value;
	}

	/* ----------------------------------------------------------------------------------------------------

		Specific curve(s)

	 ------------------------------------------------------------------------------------------------------ */
	
	// A curve Paul Coops came up with (sampled an acoustic piano if I'm not mistaken)
	// that sounds more realistic than linear key tracking for, well, I suppose, most
	// string instruments
	SFM_INLINE static float AcousticTrackingCurve(float value, float scale)
	{
		SFM_ASSERT(value >= 0.f && value <= 1.f);

//		const float scale = 1.448f;
//		const float offset = -1.23f;
//		const float postScale = 0.9f;
		
		const float linear = (value*1.448f + -1.23f);
		const float squaredClipped = fast_tanhf(linear*linear);
		
		// This value is used as a multiplier, so we want the inverse (this curve
		// only makes the envelope shorter)
		return 1.f - 0.9f*(scale * (1.f-squaredClipped));
	}
}
