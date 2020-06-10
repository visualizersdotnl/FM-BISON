
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
	
	Thank you Tammo Hinrichs for a few good tips!
*/

#include "synth-compressor.h"

namespace SFM
{	
	float Compressor::Apply(float *pLeft, float *pRight, unsigned numSamples, bool autoGain)
	{
		float bite = 0.f;

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

			m_gainEnv.SetAttack(curAttack * 1000.f);
			m_gainEnv.SetRelease(curRelease * 1000.f);

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Delay input signal
			m_outDelayL.Write(sampleL);
			m_outDelayR.Write(sampleR);
			
			// Get RMS in dB
			// Suggests that RMS isn't the best way: http://c4dm.eecs.qmul.ac.uk/audioengineering/compressors/documents/Reiss-Tutorialondynamicrangecompression.pdf (FIXME)
			const float RMS = m_RMSDetector.Run(sampleL, sampleR);
			const float signaldB = (0.f != RMS) ? Lin2dB(RMS) : kMinVolumedB;

			// Calculate slope
			SFM_ASSERT(ratio > 0.f);
			float slope = 1.f - (1.f/ratio);

			// Signal delta					
			const float deltadB = thresholddB-signaldB;
			
			// Calc. gain reduction
			float gaindB = std::min<float>(0.f, slope*deltadB);
				
			// Soft knee?
			if (kneedB > 0.f && gaindB < 0.f)
			{
				const float kneeHalfdB   = kneedB*0.5f;
				const float kneeBottomdB = thresholddB - kneeHalfdB;
				const float kneeTopdB    = thresholddB + kneeHalfdB;

				if (deltadB >= kneeBottomdB && deltadB < kneeTopdB)
				{
					// Apply soft knee (interpolate between zero compression and full compression)
					// FIXME: eliminate LangrangeInterpolation(), just simplify the calculation in-place
					float xPoints[2], yPoints[2];
					xPoints[0] = kneeBottomdB;
					xPoints[1] = kneeTopdB;
					yPoints[0] = 0.f;
					yPoints[1] = gaindB;
					gaindB = LagrangeInterpolation(xPoints, yPoints, 2, deltadB);
				}
			}

			// Apply gain envelope
			float envdB = m_gainEnv.ApplyRev(gaindB, m_gain);

			// Automatic gain (level) adjustment (nicked from: https://github.com/ptrv/auto_compressor/blob/master/Source/PluginProcessor.cpp)
			const float estimatedB = thresholddB * -slope/2.f;
			const float autodB = m_autoEnv.ApplyRev(envdB - estimatedB, m_autoSig);

			if (true == autoGain)
			{
				// Adjust gain (see above); post gain *not* applied
				envdB -= autodB + estimatedB;
			}
			else
			{
				// Apply post gain (manual)
				envdB += postGaindB;
			}
		
			// Convert to linear gain
			const float newGain = dB2Lin(envdB);

			if (gaindB < 0.f)
				bite += 1.f;

			// Apply to (delayed) signal
			const auto  delayL   = (m_outDelayL.size()-1)*lookahead;
			const auto  delayR   = (m_outDelayR.size()-1)*lookahead;
			const float delayedL = m_outDelayL.Read(delayL);
			const float delayedR = m_outDelayR.Read(delayR);

			pLeft[iSample]  = delayedL*newGain;
			pRight[iSample] = delayedR*newGain;
		}

		SFM_ASSERT(0 != numSamples);
		bite = bite/numSamples;

//		Log("Compressor bite: " + std::to_string(bite));

		return bite;
	}
}
