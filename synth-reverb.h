
/*
	FM. BISON hybrid FM synthesis -- Reverb effect based on FreeVerb.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- Supply all parameters at once using a single SetParameters() function
		- Consider using Will Pirkle's algorithm (refer to book)
*/

#pragma once

#include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"
#include "synth-oscillator.h"
#include "synth-interpolated-parameter.h"
#include "synth-delay-line.h"

namespace SFM
{
	// A specific version of a delay line; it does not own it's buffer!
	class ReverbComb
	{
	public:
		ReverbComb() :
			m_size(0)
,           m_buffer(nullptr)
,           m_writeIdx(0)
,           m_dampening(0.f)
,           m_previous(0.f)
		{
		}

		~ReverbComb() {}
		
		void SetSizeAndBuffer(size_t size, float *pBuffer)
		{
			SFM_ASSERT(size > 0);
			SFM_ASSERT(nullptr != pBuffer);
			
			m_size = size;
			m_buffer = pBuffer;
			
			Reset();
		}

		void Reset()
		{
			SFM_ASSERT(nullptr != m_buffer);
			memset(m_buffer, 0, m_size*sizeof(float));
		}
		
		void SetDampening(float value)
		{
			SFM_ASSERT(value >= 0.f && value < 1.f);
			m_dampening = value;
		}

		SFM_INLINE float Apply(float sample, float feedback = 0.f)
		{
			SFM_ASSERT(nullptr != m_buffer);
			
			const unsigned index = m_writeIdx % m_size;
			
			const float current = m_buffer[index];
			m_previous = current*(1.f-m_dampening) + (m_previous*m_dampening);
			m_buffer[index] = sample + feedback*m_previous;

			++m_writeIdx;

			return current;
		}

		// Delay is specified in samples relative to sample rate
		// Allows for LFO modulation
		SFM_INLINE float Read(float delay)
		{
			const size_t from = (m_writeIdx-int(delay)) % m_size;
			const size_t to   = (from > 0) ? from-1 : m_size-1;
			const float fraction = fracf(delay);
			const float A = m_buffer[from];
			const float B = m_buffer[to];
			const float value = lerpf<float>(A, B, fraction);
			return value;
		}

		size_t size() const { return m_size; }

	private:
		size_t m_size;
		float *m_buffer;

		unsigned m_writeIdx;
		
		float m_dampening;
		float m_previous;
	};

	// Roughly follows the same design as ReverbComb
	class ReverbAllPass
	{
	public:
		ReverbAllPass() :
			m_size(0)
,           m_buffer(nullptr)
,           m_writeIdx(0)
		{}

		~ReverbAllPass() {}

		void SetSizeAndBuffer(size_t size, float *pBuffer)
		{
			SFM_ASSERT(size > 0);
			SFM_ASSERT(nullptr != pBuffer);
			
			m_size = size;
			m_buffer = pBuffer;
			
			Reset();
		}

		void Reset()
		{
			SFM_ASSERT(nullptr != m_buffer);
			memset(m_buffer, 0, m_size*sizeof(float));
		}

		SFM_INLINE float Apply(float sample, float feedback = 0.f)
		{
			SFM_ASSERT(nullptr != m_buffer);
			
			const unsigned index = m_writeIdx % m_size;
			
			const float current = m_buffer[index];
			const float output = -sample + current;
			m_buffer[index] = sample + (current*feedback);
			
			++m_writeIdx;

			return output;
		}

		// Delay is specified in samples relative to sample rate
		// Allows for LFO modulation
		SFM_INLINE float Read(float delay)
		{
			const size_t from = (m_writeIdx-int(delay)) % m_size;
			const size_t to   = (from > 0) ? from-1 : m_size-1;
			const float fraction = fracf(delay);
			const float A = m_buffer[from];
			const float B = m_buffer[to];
			const float value = lerpf<float>(A, B, fraction);
			return value;
		}

		size_t size() const { return m_size; }

	private:
		size_t m_size;
		float *m_buffer;
		unsigned m_writeIdx;
	};

	// Warning: you can't just change these!
	constexpr unsigned kReverbNumCombs = 8;
	constexpr unsigned kReverbNumAllPasses = 4;

	// Max. room size to prevent infinite reverberation
	constexpr float kReverbMaxRoomSize = 0.9f;

	class Reverb
	{
	public:
		Reverb(unsigned sampleRate, unsigned Nyquist);
		
		~Reverb()
		{
			freeAligned(m_buffer);
		}

	private:
		SFM_INLINE void Reset()
		{
			memset(m_buffer, 0, m_totalBufSize);

			m_preLPF.resetState();
			m_preHPF.resetState();
			
			m_curWet.Set(0.f);
			m_curWidth.Set(kMinReverbWidth);
			m_curRoomSize.Set(0.f);
			m_curDampening.Set(0.f);

			const float cutHz = CutoffToHz(kDefReverbFilter, m_Nyquist);
			m_curLP.Set(cutHz);
			m_curHP.Set(cutHz);
		}
	
	public:
		SFM_INLINE void SetWidth(float width)
		{
			SFM_ASSERT(width >= SFM::kMinReverbWidth);
			m_width = width;
		}

		SFM_INLINE void SetRoomSize(float size)
		{
			SFM_ASSERT(size >= 0.f && size <= 1.f);
			size *= kReverbMaxRoomSize;
			
			// Taken from ref. implementation
			const float scale = 0.28f;
			const float offset = 0.7f;
			
			m_roomSize = (size*scale) + offset;
		}
		
		SFM_INLINE void SetDampening(float dampening)
		{
			SFM_ASSERT(dampening >= 0.f && dampening <= 1.f);
			dampening *= 0.4f; // Taken from ref. implementation
			m_dampening = dampening;			
		}

		SFM_INLINE void SetPreDelay(float timeInSec)
		{
			SFM_ASSERT(timeInSec >= 0.f && timeInSec <= kReverbPreDelayMax);
			m_preDelay = timeInSec;
		}

		// Samples are read & written sequentially so one buffer per channel suffices
		void Apply(float *pLeft, float *pRight, unsigned numSamples, float wet, float lowpass, float highpass);

	private:
		const unsigned m_sampleRate;
		const unsigned m_Nyquist;

		DelayLine m_preDelayLine;

		SvfLinearTrapOptimised2 m_preLPF, m_preHPF;

		ReverbComb m_combsL[kReverbNumCombs], m_combsR[kReverbNumCombs];
		ReverbAllPass m_allPassesL[kReverbNumAllPasses], m_allPassesR[kReverbNumAllPasses];

		// Parameters
		float m_width;
		float m_roomSize;
		float m_dampening;
		float m_preDelay;

		// Interpolated parameters
		InterpolatedParameter<kLinInterpolate> m_curWet;
		InterpolatedParameter<kLinInterpolate> m_curWidth;
		InterpolatedParameter<kLinInterpolate> m_curRoomSize;
		InterpolatedParameter<kLinInterpolate> m_curDampening;
		InterpolatedParameter<kLinInterpolate> m_curPreDelay;
		InterpolatedParameter<kLinInterpolate> m_curLP, m_curHP;

		// Single buffer is used for all passes, this likely favors cache (FIXME: check)
		size_t m_totalBufSize;
		float *m_buffer;
	};
}
