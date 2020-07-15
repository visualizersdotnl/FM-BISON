
/*
	FM. BISON hybrid FM synthesis -- Basic compressor.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
	
	Big thank you to Tammo Hinrichs for a few good tips!
*/

#include "synth-compressor.h"

namespace SFM
{	
	float Compressor::Apply(float *pLeft, float *pRight, unsigned numSamples, bool autoGain, float RMSToPeak)
	{
		SFM_ASSERT(RMSToPeak >= 0.f && RMSToPeak <= 1.f);
		
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

			// Set env. parameters in MS
			m_gainEnvdB.SetAttack(curAttack*1000.f);
			m_gainEnvdB.SetRelease(curRelease*1000.f);

			// Input
			const float sampleL = pLeft[iSample];
			const float sampleR = pRight[iSample];

			// Delay input signal
			m_outDelayL.Write(sampleL);
			m_outDelayR.Write(sampleR);
			
			// Get RMS and peak in dB
			// Ref.: http://c4dm.eecs.qmul.ac.uk/audioengineering/compressors/documents/Reiss-Tutorialondynamicrangecompression.pdf
			const float RMSdB = m_RMS.Run(sampleL, sampleR);
			const float peakdB = m_peak.Run(sampleL, sampleR);
			const float signaldB = lerpf<float>(RMSdB, peakdB, RMSToPeak);

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
					gaindB = LagrangeInterpolatef(xPoints, yPoints, 2, deltadB);
				}
			}

			float envdB = m_gainEnvdB.ApplyReverse(gaindB);

			// Automatic gain (level) adjustment (nicked from: https://github.com/ptrv/auto_compressor/blob/master/Source/PluginProcessor.cpp)
			const float estimatedB = thresholddB * -slope/2.f;
			const float autodB = m_autoGainEnvdB.Apply(envdB - estimatedB, m_autoGaindB);
//			const float autodB = m_autoGainEnvdB.Apply(envdB - estimatedB);

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
		
			// Convert to final linear gain
			const float gain = dB2Lin(envdB);

			if (gaindB < 0.f)
				bite += 1.f; // Register "bite"

			// Apply to (delayed) signal
			const float delayedL = m_outDelayL.ReadNormalized(lookahead);
			const float delayedR = m_outDelayR.ReadNormalized(lookahead);

			pLeft[iSample]  = delayedL*gain;
			pRight[iSample] = delayedR*gain;
		}

		if (numSamples > 0)
			bite = bite/numSamples;

		return bite;
	}
}
