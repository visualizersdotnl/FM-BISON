
/*
	FM. BISON hybrid FM synthesis -- Interpolated (linear or multiplicative) parameter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	This object is used to interpolate parameters that need per-sample interpolation in 
	the time domain so that it will always reproduce the same effect regardless of the
	number of samples processed per block or the sample rate. Alternatively a fixed
	number of samples can be chosen.

	Adjusting the rate requires subsequently (re)setting the (target) value; this is
	a bit of an annoyance at times (see synth-post-pass.cpp for example), so in my own
	rewrite I should address this.

	FIXME: currently I use the JUCE implementation for convenience
*/

#pragma once

// FIXME
#include "../../JuceLibraryCode/JuceHeader.h"

#include "synth-global.h"

namespace SFM
{
	typedef ValueSmoothingTypes::Linear kLinInterpolate;
	typedef ValueSmoothingTypes::Multiplicative kMulInterpolate;

	template <typename T> class InterpolatedParameter
	{
	public:
		// Default: zero
		// If you comment this constructor it's easier to spot forgotten initializations
		InterpolatedParameter()
		{
			SetRate(0);
			Set(0.f);
		}

		// Initialize at value and initialize rate & time
		InterpolatedParameter(float value, unsigned sampleRate, float time /* In sec. */) :
			m_value(value)
		{
			SFM_ASSERT(time >= 0.f);
			SetRate(sampleRate, time);
			Set(value);
		}
	
		// Initialize at value and initialize rate & time
		InterpolatedParameter(float value, unsigned numSamples) :
			m_value(value)
		{
			SFM_ASSERT(numSamples > 0);
			SetRate(numSamples);
			Set(value);
		}

		SFM_INLINE float Sample()
		{
			return m_value.getNextValue();
		}

		SFM_INLINE float Get() const
		{
			return m_value.getCurrentValue();
		}

		// Set directly (current & target)
		SFM_INLINE void Set(float value)
		{
			m_value.setCurrentAndTargetValue(value);
		}

		// Set target
		// Won't affect or reset number of steps if identical!
		SFM_INLINE void SetTarget(float value)
		{
			m_value.setTargetValue(value);
		}
		
		// Get target
		SFM_INLINE float GetTarget() const
		{
			return m_value.getTargetValue();
		}

		// Skip over N samples towards target value
		SFM_INLINE void Skip(unsigned numSamples)
		{
			m_value.skip(numSamples);
		}

		// Change ramp time adj. for rate
		SFM_INLINE void SetRate(unsigned sampleRate, float time)
		{
			m_value.reset(double(sampleRate), double(time));
		}
		
		// Change ramp in samples
		SFM_INLINE void SetRate(unsigned numSamples)
		{
			m_value.reset(numSamples);
		}

	private:
		SmoothedValue<float, T> m_value;
	};
};
