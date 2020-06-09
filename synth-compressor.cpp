
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
	
	Thank you Tammo Hinrichs for a few good tips!
*/

#include "synth-compressor.h"

namespace SFM
{
	// I stumbled across these 2 functions but I can't remember where; I'm using these two
	// functions here only, though I may want to use them as my main conversion functions? (FIXME)
	static SFM_INLINE float Lin2dB(double linear) 
	{
		constexpr double LOG_2_DB = 8.6858896380650365530225783783321; // = 20/ln(10)
		return float(log(linear)*LOG_2_DB);
	}
	
	static SFM_INLINE float dB2Lin(double dB) 
	{
		constexpr double DB_2_LOG = 0.11512925464970228420089957273422; // = ln(10)/20
		return float(exp(dB*DB_2_LOG));
	}

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

			m_envFollower.SetAttack(curAttack   *  1000.f);
			m_envFollower.SetRelease(curRelease *  1000.f);

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Delay input signal
			m_outDelayL.Write(sampleL);
			m_outDelayR.Write(sampleR);

			float adjThresholddB = thresholddB;

			// Adjust threshold for soft knee
			if (kneedB > 0.f)
				adjThresholddB -= kneedB*0.5f;

			// Calc. RMS and feed it to env. follower
			const float RMS = m_RMSDetector.Run(sampleL, sampleR);
			const float signaldB = Lin2dB(RMS);
			float deltadB = std::max<float>(0.f, signaldB-adjThresholddB);
			deltadB = m_envFollower.Apply(deltadB, m_envdB);

			// FIXME: highly questionable
			constexpr float kGain3dB = 1.41253757f;
			deltadB = std::min<float>(dBToGain(deltadB), kGain3dB) / kGain3dB;

			float adjRatio = 1.f/ratio;

			if (kneedB > 0.f)
			{
				// Soft knee
				const float kneeBlend = std::min<float>(deltadB, kneedB)/kneedB;
				adjRatio = lerpf<float>(1.f, adjRatio, smoothstepf(kneeBlend*kneeBlend));
			}

			// Adjust by ratio
			deltadB *= adjRatio-1.f;

			// https://github.com/ptrv/auto_compressor/blob/master/Source/PluginProcessor.cpp
			if (0.f == postGaindB)
			{
				const float A = 1.f - 1.f/ratio;
				const float makeUpEstimate = adjThresholddB * -A/2.f;
				m_autoFollower.Apply(deltadB - makeUpEstimate, m_autoFollow);
				deltadB -= m_autoFollow + makeUpEstimate;
			}

			// Calculate total gain
			SFM_ASSERT(ratio > 0.f);
			/* const */ float gain = dB2Lin(deltadB + postGaindB);

			if (signaldB > thresholddB)
				activity += 1.f;

			// Apply to (delayed) signal
			const auto  delayL   = m_outDelayL.size()*lookahead;
			const auto  delayR   = m_outDelayR.size()*lookahead;
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
