
/*
	FM. BISON hybrid FM synthesis -- Mini EQ (bass & treble).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
	
	" Bass and Treble is a two-band Equalizer. The Bass control is a low-shelf filter with the half 
	  gain frequency at 250 Hz. The Treble control is a high-shelf filter with the half gain frequency 
	  at 4000 Hz. " (source: Audacity)
*/

#pragma once

#include "3rdparty/filters/Biquad.h"

#include "synth-global.h"
#include "synth-interpolated-parameter.h"

namespace SFM
{
	class MiniEQ
	{
	public:
		MiniEQ(unsigned sampleRate) :
			m_bassFc(250.f/sampleRate)
,			m_trebleFc(4000.f/sampleRate)
,			m_bassdB(0.f, sampleRate, kDefParameterLatency)
,			m_trebledB(0.f, sampleRate, kDefParameterLatency)
		{
		}

		~MiniEQ() {}

		SFM_INLINE void SetTargetdBs(float bassdB, float trebledB)
		{
			SFM_ASSERT(bassdB > kInfdB);
			SFM_ASSERT(trebledB > kInfdB);

			// FIXME: I used to get an artifact when going past 0 dB
//			bassdB   += kEpsilon;
//			trebledB += kEpsilon;

			m_bassdB.SetTarget(bassdB);
			m_trebledB.SetTarget(trebledB);
		}

		SFM_INLINE void Apply(float &sampleL, float &sampleR)
		{
			SetBiquads();

			m_lowShelf.process(sampleL, sampleR);
			m_highShelf.process(sampleL, sampleR);
		}

		SFM_INLINE float ApplyMono(float sample)
		{
			SetBiquads();

			return m_highShelf.processMono(m_lowShelf.processMono(sample));
		}

	private:
		const float m_bassFc;
		const float m_trebleFc;

		Biquad m_lowShelf;
		Biquad m_highShelf;

		InterpolatedParameter<kLinInterpolate> m_bassdB;
		InterpolatedParameter<kLinInterpolate> m_trebledB;

		SFM_INLINE void SetBiquads()
		{
			m_lowShelf.setBiquad(bq_type_lowshelf, m_bassFc, 0.f, m_bassdB.Sample());       // Bass
			m_highShelf.setBiquad(bq_type_highshelf, m_trebleFc, 0.f, m_trebledB.Sample()); // Treble
		}
	};
}
