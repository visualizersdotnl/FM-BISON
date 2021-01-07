
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include "synth-global.h"
#include "synth-sidechain-envelope.h"
#include "synth-delay-line.h"
#include "synth-interpolated-parameter.h"
#include "synth-level-detect.h"

namespace SFM
{
	// Constant local parameters
	constexpr float kCompRMSWindowSec = 0.005f; // 5MS
	constexpr float kCompAutoGainMS   =    1.f; // 1MS (SignalFollower)

	class Compressor
	{
	public:

		Compressor(unsigned sampleRate) :
			m_sampleRate(sampleRate)
,			m_outDelayL(sampleRate, kMaxCompLookahead)
,			m_outDelayR(sampleRate, kMaxCompLookahead)
,			m_RMS(sampleRate, kCompRMSWindowSec)
,			m_peak(sampleRate, kMinCompAttack)
,			m_gainEnvdB(sampleRate, kInfdB)
,			m_autoGainEnvdB(sampleRate, kCompAutoGainMS)
//,			m_autoGainEnvdB(sampleRate, kInfVolumedB)
,			m_curThresholddB(kDefCompThresholddB, sampleRate, kDefParameterLatency)
,			m_curKneedB(kDefCompKneedB, sampleRate, kDefParameterLatency)
,			m_curRatio(kDefCompRatio, sampleRate, kDefParameterLatency)
,			m_curGaindB(kDefCompGaindB, sampleRate, kDefParameterLatency)
,			m_curAttack(kDefCompAttack, sampleRate, kDefParameterLatency)
,			m_curRelease(kDefCompRelease, sampleRate, kDefParameterLatency)
,			m_curLookahead(0.f, sampleRate, kDefParameterLatency)
		{
		}

		~Compressor() {}

		SFM_INLINE void SetParameters(float thresholddB, float kneedB, float ratio, float gaindB, float attack, float release, float lookahead)
		{
			SFM_ASSERT(thresholddB >= kMinCompThresholdB && thresholddB <= kMaxCompThresholdB);
			SFM_ASSERT(kneedB >= kMinCompKneedB && kneedB <= kMaxCompKneedB);
			SFM_ASSERT(ratio >= kMinCompRatio && ratio <= kMaxCompRatio);
			SFM_ASSERT(gaindB >= kMinCompGaindB && gaindB <= kMaxCompGaindB);
			SFM_ASSERT(attack >= kMinCompAttack && attack <= kMaxCompAttack);
			SFM_ASSERT(release >= kMinCompRelease && release <= kMaxCompRelease);
			SFM_ASSERT(lookahead >= 0.f && lookahead <= kMaxCompLookahead);

			m_curThresholddB.SetTarget(thresholddB);
			m_curKneedB.SetTarget(kneedB);
			m_curRatio.SetTarget(ratio);
			m_curGaindB.SetTarget(gaindB);
			m_curAttack.SetTarget(attack);
			m_curRelease.SetTarget(release);
			m_curLookahead.SetTarget(lookahead/kMaxCompLookahead); // Because DelayLine::ReadNormalized()
		}
		
		// Returns "bite" (can be used for a visual indicator)
		float Apply(float *pLeft, float *pRight, unsigned numSamples, bool autoGain, float RMSToPeak /* FIXME: interpolate as well? */);

	private:
		const unsigned m_sampleRate;

		DelayLine m_outDelayL, m_outDelayR;
		
		RMS m_RMS;
		Peak m_peak;
		FollowerEnvelope m_gainEnvdB;

//		FollowerEnvelope m_autoGainEnvdB;
		SignalFollower m_autoGainEnvdB;
		float m_autoGaindB = kInfdB;

		// Interpolated parameters
		InterpolatedParameter<kLinInterpolate> m_curThresholddB;
		InterpolatedParameter<kLinInterpolate> m_curKneedB;
		InterpolatedParameter<kLinInterpolate> m_curRatio;
		InterpolatedParameter<kLinInterpolate> m_curGaindB;
		InterpolatedParameter<kLinInterpolate> m_curAttack;
		InterpolatedParameter<kLinInterpolate> m_curRelease;
		InterpolatedParameter<kLinInterpolate> m_curLookahead;
	};
}
