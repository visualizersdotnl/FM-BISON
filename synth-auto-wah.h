
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME: 
		- Currently working on & testing (see synth-post-pass.cpp) first draft
		- Introduce wetness
		- Maybe do not use 'lookahead'
		- Generally: just find the right sound
*/

#pragma once

#include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"
#include "synth-followers.h"
#include "synth-delay-line.h"
// #include "synth-oscillator.h"

namespace SFM
{
	class AutoWah
	{
	private:

	public:
		const float kWahDelay = 0.005f; 
		const float kWahAttack = 0.01f;
		const float kWahRelease = 0.01f;
		const float kWahLookahead = 0.3f;
//		const float kWahFreq = 0.1f;

		AutoWah(unsigned sampleRate, unsigned Nyquist) :
			m_sampleRate(sampleRate), m_Nyquist(Nyquist)
,			m_outDelayL(sampleRate, kWahDelay)
,			m_outDelayR(sampleRate, kWahDelay)
,			m_detectorL(sampleRate)
,			m_detectorR(sampleRate)
,			m_gainShaper(sampleRate, kWahAttack, kWahRelease)
//,			m_LFO(sampleRate)
,			m_lookahead(kWahLookahead)
		{
//			m_LFO.Initialize(Oscillator::kSine, kWahFreq, m_sampleRate, 0.f, 0.f);
		}

		~AutoWah() {}

		SFM_INLINE void SetParameters(float attack, float release, float lookahead)
		{
			// Borrowed compressor's constants:
			SFM_ASSERT(attack >= kMinCompAttack && attack <= kMaxCompAttack);
			SFM_ASSERT(release >= kMinCompRelease && attack <= kMaxCompRelease);

			SFM_ASSERT(lookahead >= 0.f && lookahead <= 0.f);

			m_gainShaper.SetAttack(attack);
			m_gainShaper.SetRelease(release);

			m_lookahead = lookahead;
		}

		SFM_INLINE void Apply(float &left, float &right)
		{
			// Delay input signal
			m_outDelayL.Write(left);
			m_outDelayR.Write(right);

			// Detect peak using non-delayed signal
			// FIXME: might it be an idea to share this detection with Compressor?
			const float peakOutL  = m_detectorL.Apply(left);
			const float peakOutR  = m_detectorR.Apply(right);
			const float peakSum   = fast_tanhf(peakOutL+peakOutR)*0.5f; // Soft clip peak sum sounds *good*
			const float sum       = peakSum;
			const float sumdB     = (0.f != sum) ? GainTodB(sum) : kMinVolumedB;

			// Calculate gain
			const float gaindB      = m_gainShaper.Apply(sumdB);
			const float gain        = dBToGain(gaindB);
			const float clippedGain = fast_tanhf(gain);
			
			// Grab (delayed) signal
			
			// This is *correct*
//			const auto  delayL   = (m_outDelayL.size() - 1)*m_lookahead;
//			const auto  delayR   = (m_outDelayR.size() - 1)*m_lookahead; // Just in case R's size differs from L (for whatever reason)
//			const float delayedL = m_outDelayL.Read(delayL);
//			const float delayedR = m_outDelayR.Read(delayR);

			// This sounded better for Compressor, test some more! (FIXME)
			const float delayedL = lerpf<float>(left,  m_outDelayL.ReadNearest(-1), m_lookahead);
			const float delayedR = lerpf<float>(right, m_outDelayR.ReadNearest(-1), m_lookahead);

			// Calculate cutoff
			const float cutoff = CutoffToHz(clippedGain, m_Nyquist);
			
			// Filter
			float filteredL = delayedL, filteredR = delayedR;
			const float Q = 0.25f + 0.75f*ResoToQ(1.f-clippedGain);
			m_filterLP.updateCoefficients(cutoff, Q, SvfLinearTrapOptimised2::LOW_PASS_FILTER, m_sampleRate);
			m_filterLP.tick(filteredL, filteredR);

			// Add to signal
//			const float mixGain = 0.707; // Approx. -3dB
			left  = filteredL;
			right = filteredR;
		}

	private:
		const unsigned m_sampleRate;
		const unsigned m_Nyquist;

		DelayLine m_outDelayL, m_outDelayR;
		PeakDetector m_detectorL, m_detectorR;
		GainShaper m_gainShaper;
//		Oscillator m_LFO;
		SvfLinearTrapOptimised2 m_filterLP;

		// Parameters
		float m_lookahead;
	};
}

