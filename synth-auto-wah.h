
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
// #include "synth-vowelizer-V1.h"
#include "synth-vowelizer-V2.h"
#include "synth-interpolated-parameter.h"

namespace SFM
{
	class AutoWah
	{
	private:

	public:
		// Constant parameters
		const float kWahDelay = 0.005f; // 5MS             
		const float kWahLookahead = 0.3f;

		AutoWah(unsigned sampleRate, unsigned Nyquist) :
			m_sampleRate(sampleRate), m_Nyquist(Nyquist)
,			m_outDelayL(sampleRate, kWahDelay)
,			m_outDelayR(sampleRate, kWahDelay)
,			m_RMSDetector(sampleRate, 0.001f /* 1MS */)
,			m_envFollower(sampleRate, kDefWahAttack, kDefWahHold)

,			m_vowelizerV2_1(sampleRate)
,			m_vowelizerV2_2(sampleRate)

,			m_curResonance(0.f, sampleRate, kDefParameterLatency)
,			m_curAttack(kDefWahAttack, sampleRate, kDefParameterLatency)
,			m_curHold(kDefWahHold, sampleRate, kDefParameterLatency)
,			m_curRate(kDefWahRate, sampleRate, kDefParameterLatency)
,			m_curSpeak(0.f, sampleRate, kDefParameterLatency)
,			m_curCut(0.f, sampleRate, kDefParameterLatency)
,			m_curWet(0.f, sampleRate, kDefParameterLatency)
,			m_lookahead(kWahLookahead)
		{
			m_LFO.Initialize(Oscillator::Waveform::kSine, kDefWahRate, m_sampleRate, 0.f);
		}

		~AutoWah() {}

		SFM_INLINE void SetParameters(float resonance, float attack, float hold, float rate, float speak, float cut, float wetness)
		{
			SFM_ASSERT(resonance >= 0.f && resonance <= 1.f);
			SFM_ASSERT(attack >= kMinWahAttack && attack <= kMaxWahAttack);
			SFM_ASSERT(hold >= kMinWahHold && hold <= kMaxWahHold);
			SFM_ASSERT(rate >= kMinWahRate && rate <= kMaxWahRate);
			SFM_ASSERT(speak >= 0.f && speak <= 1.f);
			SFM_ASSERT(cut >= 0.f && cut <= 1.f);
			SFM_ASSERT(wetness >= 0.f && wetness <= 1.f);

			m_curResonance.SetTarget(resonance);
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
		RMSDetector m_RMSDetector;

		AttackReleaseFollower m_envFollower;
		float m_envdB = 0.f;

		SvfLinearTrapOptimised2 m_preFilterHP;
		SvfLinearTrapOptimised2 m_preFilterLP[3];
		SvfLinearTrapOptimised2 m_postFilterLP;

//		Vowelizer m_vowelizerL, m_vowelizerR;

		VowelizerV2 m_vowelizerV2_1;
		VowelizerV2 m_vowelizerV2_2;

		Oscillator m_LFO;

		// Interpolated parameters
		InterpolatedParameter<kLinInterpolate> m_curResonance;
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
