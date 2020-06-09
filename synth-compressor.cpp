
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
	
	Thank you Tammo Hinrichs for a few good tips!
*/

#include "synth-compressor.h"

namespace SFM
{	
	// FIXME: use?
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

			m_gainEnv.SetAttack(curAttack   *  1000.f);
			m_gainEnv.SetRelease(curRelease *  1000.f);

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Delay input signal
			m_outDelayL.Write(sampleL);
			m_outDelayR.Write(sampleR);
			
			// Get RMS in dB
			const float RMS = m_RMSDetector.Run(sampleL, sampleR);
			const float RMSdB = GainTodB(RMS);
			
			// Calculate slope
			SFM_ASSERT(ratio > 0.f);
			float slope = 1.f - (1.f/ratio);

			float adjThresholddB = thresholddB;
			if (kneedB > 0.f)
			{
				// Adjust slope & threshold for soft knee
				const float halfKneedB = kneedB*0.5f;
				if (RMSdB > thresholddB-halfKneedB && RMSdB < thresholddB+halfKneedB)
				{
					// Define soft knee edges
					const float kneeBottomdB = thresholddB - kneedB*0.5f;
					const float kneeTopdB    = std::fmin(kMaxCompThresholdB, thresholddB + kneedB*0.5f);

					// Calculate [0..1] delta
					const float delta = std::fmin(1.f, (RMSdB - kneeBottomdB)/kneedB);
					SFM_ASSERT(delta >= 0.f && delta <= 1.f);

					// "Schminterpolate"
					slope = lerpf<float>(0.f, slope, smoothstepf(delta*delta));

					// This is now the new zero dB adj. point
					adjThresholddB = kneeBottomdB;
				}
			}
					
			// Calc. gain reduction
			const float gaindB = std::min<float>(0.f, slope*(adjThresholddB - RMSdB));
			float envdB = m_gainEnv.ApplyRev(gaindB, m_gain);

			// Convert to linear (and add post gain)
			const float newGain = dBToGain(envdB + postGaindB);

			// Gain reduction equals activity
			if (gaindB < 0.f)
				activity += 1.f;

			// Apply to (delayed) signal
			const auto  delayL   = m_outDelayL.size()*lookahead;
			const auto  delayR   = m_outDelayR.size()*lookahead;
			const float delayedL = m_outDelayL.Read(delayL);
			const float delayedR = m_outDelayR.Read(delayR);

			pLeft[iSample]  = delayedL*newGain;
			pRight[iSample] = delayedR*newGain;
		}

		SFM_ASSERT(0 != numSamples);
		activity = activity/numSamples;

//		Log("Compressor activity: " +  std::to_string(activity));

		return activity;
	}
}
