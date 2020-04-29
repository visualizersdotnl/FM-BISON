
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME: 
		- Currently still a first draft
		- Use LFO
		- Decide on initial set of parameters & rig them
		- When rigging: make sure it uses BPM sync. as well?
		- Add the option to mix in an "aah" sound using Vowelizer?
*/

#pragma once

#include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"
#include "synth-followers.h"
#include "synth-delay-line.h"
#include "synth-oscillator.h"

namespace SFM
{
	class AutoWah
	{
	private:

	public:
		// Parameters & ranges
		const float kWahDelay = 0.003f;                // Constant (for now)
		const float kWahLookahead = kGoldenRatio*0.1f; // 
		const float kWahLFOFreq = 0.2f;                //
		const float kWahWetness = 1.f;                 //

		const float kWahPeakRelease = 0.01f;           // "Bite"  [0.01..0.1]
		const float kWahAttack = 0.01f;                // "Speed" [0.01..1.0]
		const float kWahRelease = 0.0f;                // "Hold"  [0.0..0.1]

		AutoWah(unsigned sampleRate, unsigned Nyquist) :
			m_sampleRate(sampleRate), m_Nyquist(Nyquist)
,			m_outDelayL(sampleRate, kWahDelay)
,			m_outDelayR(sampleRate, kWahDelay)
,			m_detectorL(sampleRate, kWahPeakRelease)
,			m_detectorR(sampleRate, kWahPeakRelease)
,			m_gainShaper(sampleRate, kWahAttack, kWahRelease)
,			m_wetness(kWahWetness)
,			m_lookahead(kWahLookahead)
		{
			m_LFO.Initialize(Oscillator::Waveform::kSine, kWahLFOFreq, m_sampleRate, 0.f, 0.f);
		}

		~AutoWah() {}

		SFM_INLINE void SetParameters(float bite, float speed, float hold, float rate, float wetness)
		{
			// SFM_ASSERT(bite >= kConstant && bite <= kConstant);
			SFM_ASSERT(speed >= kMinCompAttack && speed <= kMaxCompAttack); // Borrowed Compressor constants
			SFM_ASSERT(hold >= kMinCompRelease && hold <= kMaxCompRelease); //
			SFM_ASSERT(rate >= 0.f);
			SFM_ASSERT(wetness >= 0.f && wetness <= 1.f);

			m_detectorL.SetRelease(bite);
			m_detectorR.SetRelease(bite);

			m_gainShaper.SetAttack(speed);
			m_gainShaper.SetRelease(hold);

			m_LFO.SetFrequency(rate);

			m_wetness   = wetness;
			m_lookahead = kWahLookahead;
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

			float filteredL = 0.f, filteredR = 0.f;

			// Calculate global Q
			const float Q = ResoToQ(0.5f - clippedGain*0.5f);

			// Run 3 12dB low pass filters in parallel (results in a formant-like timbre)
			{
				float curCutoff = 0.1f;
				
				// Expand
				// FIXME: always spread at least a little bit?
				// FIXME: non-linear?
				const float spread = 0.3f*clippedGain;

				for (unsigned iPre = 0; iPre < 3; ++iPre)
				{
					const float cutHz = CutoffToHz(curCutoff, m_Nyquist);
					m_preFilterLP[iPre].updateLowpassCoeff(cutHz, Q, m_sampleRate);

					float filterL = delayedL, filterR = delayedR;
					m_preFilterLP[iPre].tick(filterL, filterR);

					filteredL += filterL;
					filteredR += filterR;

					curCutoff += spread;
				}

				filteredL /= 3.f;
				filteredR /= 3.f;
			}
			
			// Post filter (LP)
			const float cutHz = CutoffToHz(clippedGain*0.9f, m_Nyquist);
			m_postFilterLP.updateLowpassCoeff(cutHz, Q, m_sampleRate);
			m_postFilterLP.tick(filteredL, filteredR);
			
			// Mix
			left  = lerpf<float>(left,  filteredL, m_wetness);
			right = lerpf<float>(right, filteredR, m_wetness);
		}

	private:
		const unsigned m_sampleRate;
		const unsigned m_Nyquist;

		DelayLine m_outDelayL, m_outDelayR;
		PeakDetector m_detectorL, m_detectorR;
		GainShaper m_gainShaper;
		SvfLinearTrapOptimised2 m_preFilterLP[3];
		SvfLinearTrapOptimised2 m_postFilterLP;
		Oscillator m_LFO;

		// Local parameters
		float m_wetness;
		float m_lookahead;
	};
}
