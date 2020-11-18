
/*
	FM. BISON hybrid FM synthesis -- Mini EQ (3-band).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
	
	" Bass and Treble is a two-band Equalizer. The Bass control is a low-shelf filter with the half 
	  gain frequency at 250 Hz. The Treble control is a high-shelf filter with the half gain frequency 
	  at 4000 Hz. " (source: Audacity)

	I've added (optional) "mid" (peak filter) @ 1ooo Hz (simply spotted someone else using that frequency)
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
		MiniEQ(unsigned sampleRate, bool withMid) :
			m_withMid(withMid)
,			m_bassFc(500.f/sampleRate)
,			m_trebleFc(8000.f/sampleRate)
,			m_midFc(1000.f/sampleRate)
,			m_bassdB(0.f, sampleRate, kDefParameterLatency)
,			m_trebledB(0.f, sampleRate, kDefParameterLatency)
,			m_middB(0.f, sampleRate, kDefParameterLatency)
		{
		}

		~MiniEQ() {}

		SFM_INLINE void SetTargetdBs(float bassdB, float trebledB, float middB = 0.f)
		{
			SFM_ASSERT_RANGE(bassdB,   kTuningRangedB);
			SFM_ASSERT_RANGE(trebledB, kTuningRangedB);
			SFM_ASSERT_RANGE(middB,    kTuningRangedB);

			// FIXME: I get an artifact when going past 0 dB, figure out why
			bassdB   += kEpsilon;
			trebledB += kEpsilon;
			middB    += kEpsilon;

			m_bassdB.SetTarget(bassdB);
			m_trebledB.SetTarget(trebledB);
			m_middB.SetTarget(middB);
		}

		SFM_INLINE void Apply(float &sampleL, float &sampleR)
		{
			SetBiquads();

			if (true == m_withMid)
				m_midPeak.process(sampleL, sampleR);
				
			m_bassShelf.process(sampleL, sampleR);
			m_trebleShelf.process(sampleL, sampleR);
		}

		SFM_INLINE float ApplyMono(float sample)
		{
			SetBiquads(); 

			if (true == m_withMid)
				sample = m_midPeak.processMono(sample);

			sample = m_bassShelf.processMono(m_trebleShelf.processMono(sample));

			return sample;
		}

	private:
		const bool m_withMid;

		const float m_bassFc;
		const float m_trebleFc;
		const float m_midFc;

		Biquad m_bassShelf;
		Biquad m_trebleShelf;
		Biquad m_midPeak;

		InterpolatedParameter<kLinInterpolate> m_bassdB;
		InterpolatedParameter<kLinInterpolate> m_trebledB;
		InterpolatedParameter<kLinInterpolate> m_middB;

		SFM_INLINE void SetBiquads()
		{
			m_bassShelf.setBiquad(bq_type_highshelf, m_bassFc, 0.f, m_bassdB.Sample());      // Bass
			m_trebleShelf.setBiquad(bq_type_lowshelf, m_trebleFc, 0.f, m_trebledB.Sample()); // Treble

			// Mid?
			if (true == m_withMid)
				m_midPeak.setBiquad(bq_type_peak, m_midFc, kDefGainAtCutoff, m_middB.Sample());
		}
	};
}

