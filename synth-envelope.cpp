
/*
	FM. BISON hybrid FM synthesis -- Flexible analog style ADSR envelope.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	For information on it's shape & target ratios: https://www.youtube.com/watch?v=0oreYmOWgYE
*/

#include "synth-envelope.h"

namespace SFM
{
	SFM_INLINE static double CalcRate(double rateMul, float rate, unsigned sampleRate)
	{
		return rateMul*rate*sampleRate;
	}

	SFM_INLINE static double CalcRatio(float curve)
	{
		SFM_ASSERT(curve >= 0.f && curve <= 1.f);
		
		// Tweaked version of function in Nigel's own widget (https://www.earlevel.com/main/2013/06/23/envelope-generators-adsr-widget/)
		const double invX = 1.0 - curve;
		return kEpsilon + 0.03 * (exp(3.6*invX) - 1.0);
	}

	void Envelope::Start(const Parameters &parameters, unsigned sampleRate, bool isCarrier, float keyTracking, float velScaling)
	{
		SFM_ASSERT(parameters.rateMul >= kEnvMulMin && parameters.rateMul <= kEnvMulMax);
		SFM_ASSERT(keyTracking >= 0.f && keyTracking <= 1.f);
		SFM_ASSERT(velScaling >= 1.f);

		SFM_ASSERT(parameters.attackCurve  >= 0.f && parameters.attackCurve  <= 1.f);
		SFM_ASSERT(parameters.decayCurve   >= 0.f && parameters.decayCurve   <= 1.f);
		SFM_ASSERT(parameters.releaseCurve >= 0.f && parameters.releaseCurve <= 1.f);
		
		// Reset
		m_preAttackSamples = unsigned(parameters.preAttack*sampleRate);
		m_ADSR.reset();

		// Set ratios
		const double ratioA = CalcRatio(parameters.attackCurve);
		const double ratioD = CalcRatio(parameters.decayCurve);
		const double ratioR = CalcRatio(parameters.releaseCurve);

		m_ADSR.setTargetRatioA(ratioA);
		m_ADSR.setTargetRatioD(ratioD);
		m_ADSR.setTargetRatioR(ratioR);
		
		// Set sustain
		SFM_ASSERT(parameters.sustain >= 0.f && parameters.sustain <= 1.f);
		m_ADSR.setSustainLevel(parameters.sustain);

		// Set rates
		const double rateMul  = parameters.rateMul*keyTracking;
		const double attack   = CalcRate(rateMul,            parameters.attack,  sampleRate);
		const double decay    = CalcRate(rateMul*velScaling, parameters.decay,   sampleRate); // Velocity scaling can lengthen the decay phase
		const double release  = CalcRate(rateMul,            parameters.release, sampleRate);

		m_ADSR.setAttackRate(attack);
		m_ADSR.setDecayRate(decay);
		m_ADSR.setReleaseRate(release);
		
		// Go!
		m_ADSR.gate(true, 0.f);

		// Maarten van Strien advised this due to his experience with FM8
		// It means the envelope will never go past it's sustain phase
		// This does however not apply if this envelope is part of a carrier operator!
		m_isInfinite = false == isCarrier && parameters.release == 1.f;
	}

	void Envelope::OnPianoSustain(unsigned sampleRate, float falloff, float releaseRateMul)
	{	
		SFM_ASSERT(falloff >= 0.f && falloff <= 1.f);
		SFM_ASSERT(releaseRateMul >= kPianoPedalMinReleaseMul && releaseRateMul <= kPianoPedalMaxReleaseMul);

		//
		// - Sustains at current output level
		// - Sustain falloff defines the curvature (and length) of the sustain phase
		// - Release phase duration (rate) can be elongated by a parameter
		//

		switch (m_ADSR.getState())
		{
		case ADSR::env_idle:
			break;

		case ADSR::env_attack:
		case ADSR::env_decay:
		case ADSR::env_sustain:
			{
				// Set curvature (also affects length) of sustain phase
				m_ADSR.pianoSustain(sampleRate, CalcRatio(0.5f + 0.5f*falloff /* Lower range isn't useful nor realistic */));

				// Equal to longer release rate
				const double releaseRate = m_ADSR.getReleaseRate();

				m_ADSR.setReleaseRate(releaseRate*releaseRateMul);
				
				// At this point I wanted to adjust the release ratio to a fully linear curve, but I later realized it's 
				// up to the patch to decide how the release of the instrument should behave, so should I offer the option
				// above at all?
			}

			break;

		case ADSR::env_piano_sustain:
			break;

		case ADSR::env_release:
			break;

		default:
			SFM_ASSERT(false);
			break;
		}
	}
}
