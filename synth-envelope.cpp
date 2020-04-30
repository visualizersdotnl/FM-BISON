
/*
	FM. BISON hybrid FM synthesis -- Flexible ADSR envelope.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	For information on it's shape & target ratios: https://www.youtube.com/watch?v=0oreYmOWgYE
*/

#include "synth-envelope.h"

namespace SFM
{
	SFM_INLINE static float CalcRate(float rateMul, float rate, unsigned sampleRate)
	{
		return rateMul*rate*sampleRate;
	}

	SFM_INLINE static float CalcRatio(float curve)
	{
		SFM_ASSERT(curve >= 0.f && curve <= 1.f);
		
		curve = 1.f-curve;
		
		// Function taken from Nigel's own widget (https://www.earlevel.com/main/2013/06/23/envelope-generators-adsr-widget/)
		return 0.001f * (expf(12.f*curve)-1.f);
	}

	void Envelope::Start(const Parameters &parameters, unsigned sampleRate, bool isCarrier, float keyScaling, float velScaling, float outputOnAttack)
	{
		SFM_ASSERT(parameters.rateMul >= kEnvMulMin && parameters.rateMul <= kEnvMulMax);
		SFM_ASSERT(keyScaling >= 0.f && keyScaling <= 1.f);
		SFM_ASSERT(velScaling >= 1.f); // && velScaling <= 2.f);
		SFM_ASSERT(outputOnAttack >= 0.f && outputOnAttack <= 1.f);

		SFM_ASSERT(parameters.attackCurve  >= 0.f && parameters.attackCurve  <= 1.f);
		SFM_ASSERT(parameters.decayCurve   >= 0.f && parameters.decayCurve   <= 1.f);
		SFM_ASSERT(parameters.releaseCurve >= 0.f && parameters.releaseCurve <= 1.f);
		
		// Reset
		m_preAttackSamples = unsigned(parameters.preAttack*sampleRate);
		m_ADSR.reset();

		// Set ratios
		const float ratioA = CalcRatio(parameters.attackCurve);
		const float ratioD = CalcRatio(parameters.decayCurve);
		const float ratioR = CalcRatio(parameters.releaseCurve);

		m_ADSR.setTargetRatioA(ratioA);
		m_ADSR.setTargetRatioD(ratioD);
		m_ADSR.setTargetRatioR(ratioR);
		
		// Set sustain
		SFM_ASSERT(parameters.sustain >= 0.f && parameters.sustain <= 1.f);
		m_ADSR.setSustainLevel(parameters.sustain);

		// Set rates
		const float rateMul = parameters.rateMul*keyScaling; // For ex. key scaling can make the envelope shorter

		const float attack   = CalcRate(rateMul, parameters.attack, sampleRate);
		const float decay    = CalcRate(rateMul*velScaling, parameters.decay, sampleRate);   // Velocity scaling can lengthen the decay phase
		const float release  = CalcRate(rateMul/velScaling, parameters.release, sampleRate); // Velocity scaling can shorten the attack phase

		m_ADSR.setAttackRate(attack);
		m_ADSR.setDecayRate(decay);
		m_ADSR.setReleaseRate(release);
	
		m_ADSR.gate(true, outputOnAttack);

		// Maarten van Strien advised this due to his experience with FM8
		// It means the envelope will never go past it's sustain phase
		// This does however not apply if this envelope is part of a carrier operator!
		m_isInfinite = false == isCarrier && parameters.release == 1.f;
	}

	void Envelope::OnPianoSustain(unsigned sampleRate, double falloff, double releaseRateMul)
	{	
		SFM_ASSERT(falloff >= 0.0 && falloff <= 1.0);
		SFM_ASSERT(releaseRateMul >= kPianoPedalMinReleaseMul && releaseRateMul <= kPianoPedalMaxReleaseMul);

		//
		// - Sustains at current output level
		// - Sustain falloff defines the curvature (and length) of the sustain phase
		// - Release phase duration (rate) can be elongated by a parameter
		// - Release phase curve is set to linear
		//

		switch (m_ADSR.getState())
		{
		case ADSR::env_idle:
			break;

		case ADSR::env_attack:
		case ADSR::env_decay:
		case ADSR::env_sustain:
			{
				// Falloff simply means curvature
				const double adjFalloff = 1.0 - 0.5*falloff; // More faloff means a sharper and quicker decay (range limited because at some point it's just *too* long)
				const double falloffRatio =  0.001 * (exp(12.0*adjFalloff)-1.0); // Copied from CalcRatio()
				m_ADSR.pianoSustain(sampleRate, falloffRatio);

				// Equal to longer release rate
				const double releaseRate = m_ADSR.getReleaseRate();
				m_ADSR.setReleaseRate(releaseRate*releaseRateMul);

				// Linear release curve
				m_ADSR.setTargetRatioR(CalcRatio(0.f)); 
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
