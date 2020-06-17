
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"
#include "synth-sidechain-envelope.h"
#include "synth-delay-line.h"
#include "synth-oscillator.h"
#include "synth-DX7-LFO-table.h"
#include "quarantined/synth-vowelizer-V1.h"
#include "synth-interpolated-parameter.h"
#include "synth-level-detect.h"

namespace SFM
{
	// Local constant parameters
	constexpr float kWahRMSWindowSec = 0.002f;     // 2MS
	constexpr float kWahGhostAttackMS = 10.f;      // 10MS
	constexpr float kMinWahGhostReleaseMS = 200.f; // 200MS
	constexpr float kMaxWahGhostReleaseMS = 600.f; // 500MS
	constexpr float kWahVoxSandHSlewRate = 0.001f; // 1MS

	class AutoWah
	{
	private:

	public:
		AutoWah(unsigned sampleRate, unsigned Nyquist) :
			m_sampleRate(sampleRate), m_Nyquist(Nyquist)
,			m_RMS(sampleRate, kWahRMSWindowSec)
,			m_sideEnv(sampleRate, kDefWahAttack, kDefWahHold)
,			m_voxSandH(sampleRate)
,			m_voxGhostEnv(sampleRate, kWahGhostAttackMS, kMaxWahGhostReleaseMS /* Default */)
,			m_curResonance(0.f, sampleRate, kDefParameterLatency)
,			m_curAttack(kDefWahAttack, sampleRate, kDefParameterLatency)
,			m_curHold(kDefWahHold, sampleRate, kDefParameterLatency)
,			m_curRate(kDefWahRate, sampleRate, kDefParameterLatency)
,			m_curSpeak(0.f, sampleRate, kDefParameterLatency)
,			m_curCut(0.f, sampleRate, kDefParameterLatency)
,			m_curWet(0.f, sampleRate, kDefParameterLatency)
		{
			m_voxOscPhase.Initialize(kDefWahRate, sampleRate);
			m_LFO.Initialize(Oscillator::Waveform::kTriangle, kDefWahRate, m_sampleRate, 0.f);
		}

		~AutoWah() {}

		SFM_INLINE void SetParameters(float resonance, float attack, float hold, float rate, float speak, float speakVowel, float speakVowelMod, float speakGhost, float cut, float wetness)
		{
			SFM_ASSERT(resonance >= 0.f && resonance <= 1.f);
			SFM_ASSERT(attack >= kMinWahAttack && attack <= kMaxWahAttack);
			SFM_ASSERT(hold >= kMinWahHold && hold <= kMaxWahHold);
			SFM_ASSERT(rate >= kMinWahRate && rate <= kMaxWahRate);
			SFM_ASSERT(speak >= 0.f && speak <= 1.f);
			SFM_ASSERT(speakVowel >= 0.f && speakVowel <= kMaxWahSpeakVowel);
			SFM_ASSERT(speakVowelMod >= 0.f && speakVowelMod <= 1.f);
			SFM_ASSERT(speakGhost >= 0.f && speakGhost <= 1.f);
			SFM_ASSERT(cut >= 0.f && cut <= 1.f);
			SFM_ASSERT(wetness >= 0.f && wetness <= 1.f);

			m_curResonance.SetTarget(resonance);
			m_curAttack.SetTarget(attack);
			m_curHold.SetTarget(hold);
			m_curRate.SetTarget(rate);
			m_curSpeak.SetTarget(speak);
			m_curSpeakVowel.SetTarget(speakVowel);
			m_curSpeakVowelMod.SetTarget(speakVowelMod);
			m_curSpeakGhost.SetTarget(speakGhost);
			m_curCut.SetTarget(cut);
			m_curWet.SetTarget(wetness);
		}

		void Apply(float *pLeft, float *pRight, unsigned numSamples);

	private:
		const unsigned m_sampleRate;
		const unsigned m_Nyquist;

		RMS m_RMS;

		FollowerEnvelope m_sideEnv;

		SvfLinearTrapOptimised2 m_preFilterHP;
		SvfLinearTrapOptimised2 m_preFilterLP[3];
		SvfLinearTrapOptimised2 m_postFilterLP;
		
		Phase m_voxOscPhase;
		SampleAndHold m_voxSandH;
		FollowerEnvelope m_voxGhostEnv;
		VowelizerV1 m_vowelizerV1;

		Oscillator m_LFO;

		// Interpolated parameters
		InterpolatedParameter<kLinInterpolate> m_curResonance;
		InterpolatedParameter<kLinInterpolate> m_curAttack;
		InterpolatedParameter<kLinInterpolate> m_curHold;
		InterpolatedParameter<kLinInterpolate> m_curRate;
		InterpolatedParameter<kLinInterpolate> m_curSpeak;
		InterpolatedParameter<kLinInterpolate> m_curSpeakVowel;
		InterpolatedParameter<kLinInterpolate> m_curSpeakVowelMod;
		InterpolatedParameter<kLinInterpolate> m_curSpeakGhost;
		InterpolatedParameter<kLinInterpolate> m_curCut;
		InterpolatedParameter<kLinInterpolate> m_curWet;
	};
}
