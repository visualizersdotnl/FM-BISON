
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) njdewit technologies (visualizers.nl) & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Lookahead is a tricky concept:
	- Full lookahead (kMaxCompLookaheadMS) means *direct* compressor response.
	- This is because lookahead is implemented using a delay.
*/

#pragma once

#include "synth-global.h"
#include "synth-signal-follower.h"
#include "synth-delay-line.h"
#include "synth-interpolated-parameter.h"
#include "synth-level-detect.h"

namespace SFM
{
	// Constant local parameters
	constexpr float kCompRMSWindowSec      = 0.400f; // 400MS (EBU R 128 'Momentary', https://tech.ebu.ch/docs/tech/tech3341.pdf)
	constexpr float kCompLookaheadMS       =   10.f; //  10MS (5MS-10MS seems to be an acceptable range in the audio world)
	constexpr float kCompAutoGainSlewInSec = 0.100f; // 100MS

	class Compressor
	{
	public:

		Compressor(unsigned sampleRate) :
			m_sampleRate(sampleRate)
,			m_outDelayL(sampleRate, kCompLookaheadMS*0.001f)
,			m_outDelayR(sampleRate, kCompLookaheadMS*0.001f)
,			m_RMS(sampleRate, kCompRMSWindowSec)
,			m_peak(sampleRate, kMinCompAttack)
,			m_gainEnvdB(sampleRate, 0.f /* Unit gain in dB */)
,			m_curThresholddB(kDefCompThresholddB, sampleRate, kDefParameterLatency)
,			m_curKneedB(kDefCompKneedB, sampleRate, kDefParameterLatency)
,			m_curRatio(kDefCompRatio, sampleRate, kDefParameterLatency)
,			m_curGaindB(kDefCompGaindB, sampleRate, kDefParameterLatency)
,			m_curAttack(kDefCompAttack, sampleRate, kDefParameterLatency)
,			m_curRelease(kDefCompRelease, sampleRate, kDefParameterLatency)
,			m_curLookahead(0.f, sampleRate, kDefParameterLatency)
,			m_autoGainCoeff(expf(-1.f / (sampleRate*kCompAutoGainSlewInSec)))
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
			SFM_ASSERT_NORM(lookahead);

			m_curThresholddB.SetTarget(thresholddB);
			m_curKneedB.SetTarget(kneedB);
			m_curRatio.SetTarget(ratio);
			m_curGaindB.SetTarget(gaindB);
			m_curAttack.SetTarget(attack);
			m_curRelease.SetTarget(release);
			m_curLookahead.SetTarget(lookahead);
		}
		
		// Returns "bite" (can be used for a visual indicator)
		float Apply(float *pLeft, float *pRight, unsigned numSamples, bool autoGain, float RMSToPeak /* FIXME: interpolate as well? */);

		SFM_INLINE float GetLatency() const
		{
			const float lookaheadInSec = m_curLookahead.Get()*kCompLookaheadMS * 0.001f;
			return m_sampleRate*lookaheadInSec;
		}

	private:
		const unsigned m_sampleRate;

		DelayLine m_outDelayL, m_outDelayR;
		
		RMS m_RMS;
		Peak m_peak;
		FollowerEnvelope m_gainEnvdB;

		const float m_autoGainCoeff;
		float m_autoGainDiff = 0.f;

		// Interpolated parameters
		InterpolatedParameter<kLinInterpolate, false> m_curThresholddB;
		InterpolatedParameter<kLinInterpolate, false> m_curKneedB;
		InterpolatedParameter<kLinInterpolate, true, kMinCompRatio, kMaxCompRatio> m_curRatio;
		InterpolatedParameter<kLinInterpolate, false> m_curGaindB;
		InterpolatedParameter<kLinInterpolate, true, kMinCompAttack, kMaxCompAttack> m_curAttack;
		InterpolatedParameter<kLinInterpolate, true, kMinCompRelease, kMaxCompRelease> m_curRelease;
		InterpolatedParameter<kLinInterpolate, true> m_curLookahead;
	};
}
