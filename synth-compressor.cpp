
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
	
	Thank you Tammo Hinrichs for a few good tips!
*/

#include "synth-compressor.h"

namespace SFM
{
	float Compressor::Apply(float *pLeft, float *pRight, unsigned numSamples)
	{
		float activity = 0.f;

		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			// Get parameters
			const float thresholddB = m_curThresholddB.Sample();
			const float ratio       = m_curRatio.Sample();
			const float postGaindB  = m_curGaindB.Sample();
			const float lookahead   = m_curLookahead.Sample();
			const float curAttack   = m_curAttack.Sample();
			const float curRelease  = m_curRelease.Sample();
			const float kneedB      = m_curKneedB.Sample();

			m_envFollower.SetAttack(curAttack   *  100.f);
			m_envFollower.SetRelease(curRelease *  1000.f);

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Delay input signal
			m_outDelayL.Write(sampleL);
			m_outDelayR.Write(sampleR);

			// Adjust threshold for soft knee
			float adjThresholddB = thresholddB;
			if (kneedB > 0.f)
				adjThresholddB -= kneedB*0.5f;

			// Calc. RMS and feed it to env. follower
			const float RMS = m_RMSDetector.Run(sampleL, sampleR);
			const float signaldB = (RMS != 0.f) ? GainTodB(RMS) : kMinVolumedB;
			float deltadB = std::max<float>(0.f, signaldB-adjThresholddB);
			deltadB = m_envFollower.Apply(deltadB, m_envdB);

			float adjRatio = ratio;
			if (kneedB > 0.f)
			{
				// Soft knee
				const float kneeBlend = std::min<float>(deltadB, kneedB)/kneedB;
				adjRatio = lerpf<float>(1.f, ratio, kneeBlend*kneeBlend);
			}
			
			// Calculate total gain
			SFM_ASSERT(ratio > 0.f);
			const float gaindB = -deltadB*(1.f - 1.f/adjRatio);
			const float gain = dBToGain(gaindB + postGaindB);

			if (signaldB > thresholddB)
				activity += 1.f;

			// Apply to (delayed) signal
			const auto  delayL   = (m_outDelayL.size()-1)*lookahead;
			const auto  delayR   = (m_outDelayR.size()-1)*lookahead;
			const float delayedL = m_outDelayL.Read(delayL);
			const float delayedR = m_outDelayR.Read(delayR);

			pLeft[iSample]  = delayedL*gain;
			pRight[iSample] = delayedR*gain;
		}

		// There's absolutely zero science going on here:
		SFM_ASSERT(0 != numSamples);
		activity = activity/numSamples;

//		Log("Compressor activity: " +  std::to_string(activity));

		return activity;
	}
}
