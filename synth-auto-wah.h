
/*
	FM. BISON hybrid FM synthesis -- "auto-wah" implementation (WIP).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- I've corrected for the high cut a bit so the effect does not diminish too fast, but it's late, and it can probably be done better
		- Use BPM sync.?
*/

#pragma once

#include "3rdparty/SvfLinearTrapOptimised2.hpp"

#include "synth-global.h"
#include "synth-followers.h"
#include "synth-delay-line.h"
#include "synth-oscillator.h"
#include "synth-DX7-LFO-table.h"
#include "synth-vowelizer.h"

namespace SFM
{
	class AutoWah
	{
	private:

	public:
		// Constant parameters
		const float kWahDelay = 0.010f;                
		const float kWahLookahead = kGoldenRatio*0.1f;

/*
		const float kWahPeakRelease = 0.01f;           // "Slack"  [0.01..0.1] -> How fast (new) peaks are detected
		const float kWahAttack = 0.01f;                // "Speed"  [0.01..1.0] -> How fast an upwards change in peak gains traction
		const float kWahRelease = 0.1f;                // "Hold"   [0.0..0.1]  -> How long current peak is held
		const float kWahLFOFreq = kDX7_LFO_To_Hz[4];   // "Rate"               -> LFO rate
		const float kWahHiCut = 0.05f;                 // "Cut"    [0.0..1.0]  -> High pass cutoff (remainder is carried)
		const float kWahVowelize = 0.f;                // "Speak"  [0.0..1.0]  -> Wet/dry of the actual formant filter (last step)
		const float kWahWetness = 1.f;                 // [0..1]
*/

		AutoWah(unsigned sampleRate, unsigned Nyquist) :
			m_sampleRate(sampleRate), m_Nyquist(Nyquist)
,			m_outDelayL(sampleRate, kWahDelay)
,			m_outDelayR(sampleRate, kWahDelay)
,			m_detectorL(sampleRate, kDefWahSlack)
,			m_detectorR(sampleRate, kDefWahSlack)
,			m_gainShaper(sampleRate, kDefWahSpeed, kDefWahHold)
,			m_vowelize(0.f)
,			m_hiCut(0.f)
,			m_wetness(0.f)
,			m_lookahead(kWahLookahead)
		{
			m_LFO.Initialize(Oscillator::Waveform::kSine, kDefWahRate, m_sampleRate, 0.f, 0.f);
		}

		~AutoWah() {}

		SFM_INLINE void SetParameters(float slack, float speed, float hold, float rate, float speak, float cut, float wetness)
		{
			SFM_ASSERT(slack >= kMinWahSlack && slack <= kMaxWahSlack);
			SFM_ASSERT(speed >= kMinWahSpeed && speed <= kMaxWahSpeed);
			SFM_ASSERT(hold >= kMinWahHold && hold <= kMaxWahHold);
			SFM_ASSERT(rate >= kMinWahRate && rate <= kMaxWahRate);
			SFM_ASSERT(speak >= 0.f && speak <= 1.f);
			SFM_ASSERT(cut >= 0.f && cut <= 1.f);
			SFM_ASSERT(wetness >= 0.f && wetness <= 1.f);

			m_detectorL.SetRelease(slack);
			m_detectorR.SetRelease(slack);

			m_gainShaper.SetAttack(speed);
			m_gainShaper.SetRelease(hold);

			m_LFO.SetFrequency(rate);

			m_vowelize  = smoothstepf(speak);
			m_hiCut     = cut*0.125f; // Nyquist/8 is more than enough!
			m_wetness   = wetness;
			m_lookahead = kWahLookahead;
		}

		SFM_INLINE void Apply(float &left, float &right)
		{
			// Delay input signal
			m_outDelayL.Write(left);
			m_outDelayR.Write(right);

			// Detect peak using non-delayed signal
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
			float delayedL = lerpf<float>(left,  m_outDelayL.ReadNearest(-1), m_lookahead);
			float delayedR = lerpf<float>(right, m_outDelayR.ReadNearest(-1), m_lookahead);

			// Cut off high end and that's what we'll work with
			float preFilteredL = delayedL, preFilteredR = delayedR;
			m_preFilterHP.updateCoefficients(CutoffToHz(m_hiCut, m_Nyquist), 0.025, SvfLinearTrapOptimised2::HIGH_PASS_FILTER, m_sampleRate);
			m_preFilterHP.tick(preFilteredL, preFilteredR);

			// Store remainder to add back into mix
			const float remainderL = delayedL-preFilteredL;
			const float remainderR = delayedR-preFilteredR;

			// Replace
			delayedL = preFilteredL;
			delayedR = preFilteredR;

			float filteredL = 0.f, filteredR = 0.f;

			// Run 3 12dB low pass filters in parallel (results in a formant-like timbre)
			{
				float curCutoff = 0.1f + m_hiCut;
				const float Q = ResoToQ(0.5f - clippedGain*0.5f);
				
				// Expand
				// FIXME: always spread at least a little bit?
				// FIXME: non-linear?
				const float spread = (0.3f - m_hiCut/3.f)*clippedGain;

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
				
				// Normalize
				filteredL /= 3.f;
				filteredR /= 3.f;
			}
			
			// Post filter (LP)
			// FIXME: hardcoded values, I feel a tad unhappy about this, though it can also be regarded as Stijn's credo: dare to make choices
			const float LFO = 0.09f*m_LFO.Sample(0.f);
			const float cutoff = std::max<float>(0.f, clippedGain*0.9f + LFO);
			const float cutHz = CutoffToHz(cutoff, m_Nyquist);
			const float Q = ResoToQ(0.9f - (0.2f*m_wetness + clippedGain*0.5f));
			m_postFilterLP.updateLowpassCoeff(cutHz, Q, m_sampleRate);
			m_postFilterLP.tick(filteredL, filteredR);

			// Add (low) remainder to signal
			const float postFilterL = remainderL+filteredL;
			const float postFilterR = remainderR+filteredR;

			// Vowelize ("AAH" -> "UUH")
			const float vowel_L = m_vowelizerL.Apply(postFilterL*0.5f, Vowelizer::kA, clippedGain);
			const float vowel_R = m_vowelizerR.Apply(postFilterR*0.5f, Vowelizer::kA, clippedGain);
			
			// Blend to final filtered mix
			const float finalL = lerpf<float>(postFilterL, vowel_L, m_vowelize);
			const float finalR = lerpf<float>(postFilterR, vowel_R, m_vowelize);

			// Mix with dry signal
			left  = lerpf<float>(left,  finalL, m_wetness);
			right = lerpf<float>(right, finalR, m_wetness);
		}

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

		// Local parameters
		float m_vowelize;
		float m_hiCut;
		float m_wetness;
		float m_lookahead;
	};
}
