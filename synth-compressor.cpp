
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
		SFM_ASSERT_NORM(RMSToPeak);
		
		float bite = 0.f;

		for (unsigned iSample = 0; iSample < numSamples; ++iSample)
		{
			// Get parameters
			/* const */ float thresholddB = m_curThresholddB.Sample();
			const float ratio             = m_curRatio.Sample();
			const float postGaindB        = m_curGaindB.Sample();
			const float lookahead         = m_curLookahead.Sample();
			const float curAttack         = m_curAttack.Sample();
			const float curRelease        = m_curRelease.Sample();
			const float kneedB            = m_curKneedB.Sample();

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

			// Soft knee?
			float kneeMul = 1.f;
			if (kneedB > 0.f)
			{
				const float kneeHalfdB   = kneedB*0.5f;
				const float kneeTopdB    = thresholddB + kneeHalfdB;
				const float kneeBottomdB = thresholddB - kneeHalfdB;

				if (signaldB >= kneeBottomdB && signaldB < kneeTopdB)
				{
					// Apply (pragmatic) soft knee
					kneeMul = easeInOutQuintf((signaldB-kneeBottomdB)/kneedB);
				}

				thresholddB = kneeBottomdB;
			}

			// Signal delta					
			const float deltadB = thresholddB-signaldB;

			// Calc. gain reduction
			float gaindB = std::min<float>(0.f, slope*deltadB*kneeMul);

			float envdB = m_gainEnvdB.ApplyReverse(gaindB);

			// Adjust gain		
			float makeUpGain = 1.f;
			if (true == autoGain)
			{
				// FIXME: can use coefficient here
				const float makeUpdB = fabsf(thresholddB/ratio);
				makeUpGain = dB2Lin(makeUpdB);
			}
			else
			{
				// Apply post gain (manual)
				envdB += postGaindB;
			}
		
			// Convert to final linear gain
			const float gain = dB2Lin(envdB) * makeUpGain;

			if (deltadB < 0.f)
				bite += 1.f; // Register "bite"

			// Apply to (delayed) signal
			const float invLookahead = 1.f-lookahead;
			const float delayedL = m_outDelayL.ReadNormalized(invLookahead);
			const float delayedR = m_outDelayR.ReadNormalized(invLookahead);

			pLeft[iSample]  = delayedL*gain;
			pRight[iSample] = delayedR*gain;
		}

		if (numSamples > 0)
		{
			bite = bite/numSamples;
			SFM_ASSERT_NORM(bite);
		}

		return bite;
	}
}
