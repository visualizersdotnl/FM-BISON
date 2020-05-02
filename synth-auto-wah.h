
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"
#include "synth-followers.h"
#include "synth-delay-line.h"
#include "synth-oscillator.h"
#include "synth-DX7-LFO-table.h"
#include "synth-vowelizer.h"
#include "synth-interpolated-parameter.h"

namespace SFM
{
	class AutoWah
	{
	private:

	public:
		// Constant parameters
		constexpr float kWahDelay = 0.01f; // 10MS             
		constexpr float kWahLookahead = kGoldenRatio*0.1f; // Arbitrary, sounds OK

		AutoWah(unsigned sampleRate, unsigned Nyquist) :
			m_sampleRate(sampleRate), m_Nyquist(Nyquist)
,			m_outDelayL(sampleRate, kWahDelay)
,			m_outDelayR(sampleRate, kWahDelay)
,			m_detectorL(sampleRate, kDefWahSlack)
,			m_detectorR(sampleRate, kDefWahSlack)
,			m_gainShaper(sampleRate, kDefWahAttack, kDefWahHold)
,			m_curSlack(kDefWahSlack, sampleRate, kDefParameterLatency)
,			m_curAttack(kDefWahAttack, sampleRate, kDefParameterLatency)
,			m_curHold(kDefWahHold, sampleRate, kDefParameterLatency)
,			m_curRate(kDefWahRate, sampleRate, kDefParameterLatency)
,			m_curSpeak(0.f, sampleRate, kDefParameterLatency)
,			m_curCut(0.f, sampleRate, kDefParameterLatency)
,			m_curWet(0.f, sampleRate, kDefParameterLatency)
,			m_lookahead(kWahLookahead)
		{
			m_LFO.Initialize(Oscillator::Waveform::kSine, kDefWahRate, m_sampleRate, 0.f, 0.f);
		}

		~AutoWah() {}

		SFM_INLINE void SetParameters(float slack, float attack, float hold, float rate, float speak, float cut, float wetness)
		{
			SFM_ASSERT(slack >= kMinWahSlack && slack <= kMaxWahSlack);
			SFM_ASSERT(attack >= kMinWahAttack && attack <= kMaxWahAttack);
			SFM_ASSERT(hold >= kMinWahHold && hold <= kMaxWahHold);
			SFM_ASSERT(rate >= kMinWahRate && rate <= kMaxWahRate);
			SFM_ASSERT(speak >= 0.f && speak <= 1.f);
			SFM_ASSERT(cut >= 0.f && cut <= 1.f);
			SFM_ASSERT(wetness >= 0.f && wetness <= 1.f);

			m_curSlack.SetTarget(slack);
			m_curAttack.SetTarget(attack);
			m_curHold.SetTarget(hold);
			m_curRate.SetTarget(rate);
			m_curSpeak.SetTarget(speak);
			m_curCut.SetTarget(cut);
			m_curWet.SetTarget(wetness);
		}

		void Apply(float *pLeft, float *pRight, unsigned numSamples);

	private:
		const unsigned m_sampleRate;
		const unsigned m_Nyquist;

		DelayLine m_outDelayL, m_outDelayR;
		PeakDetector m_detectorL, m_detectorR;
		GainShaper m_gainShaper;
		SvfLinearTrapOptimised2 m_preFilterHP;
		SvfLinearTrapOptimised2 m_preFilterLP[3];
		SvfLinearTrapOptimised2 m_postFilterLP;
		Oscillator m_LFO;
		Vowelizer m_vowelizerL, m_vowelizerR;
		
		// Interpolated parameters
		InterpolatedParameter<kLinInterpolate> m_curSlack;
		InterpolatedParameter<kLinInterpolate> m_curAttack;
		InterpolatedParameter<kLinInterpolate> m_curHold;
		InterpolatedParameter<kLinInterpolate> m_curRate;
		InterpolatedParameter<kLinInterpolate> m_curSpeak;
		InterpolatedParameter<kLinInterpolate> m_curCut;
		InterpolatedParameter<kLinInterpolate> m_curWet;

		// Constant parameter(s)
		const float m_lookahead;
	};
}
