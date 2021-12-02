
/*
	FM. BISON hybrid FM synthesis -- Flexible analog style ADSR envelope.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- Introduce non-linear interpolation, an attack level et cetera (discuss w/Paul)
*/

#include "synth-envelope.h"

namespace SFM
{
	void Envelope::Start(const Parameters &parameters, unsigned sampleRate, bool isCarrier, float keyTracking, float velScaling)
	{
		SFM_ASSERT(parameters.globalMul >= kEnvMulMin && parameters.globalMul <= kEnvMulMax);
		SFM_ASSERT_NORM(keyTracking);
		SFM_ASSERT(velScaling >= 1.f);

		SFM_ASSERT_NORM(parameters.attackCurve);
		SFM_ASSERT_NORM(parameters.decayCurve);
		SFM_ASSERT_NORM(parameters.releaseCurve);
		SFM_ASSERT_NORM(parameters.sustain);

		// Reset
		m_ADSR.reset();

		m_preAttackSamples = unsigned(parameters.preAttack*sampleRate);

		// Global multiplier (in seconds), itself multiplied by the key tracking value
		const float globalMul = parameters.globalMul*keyTracking;

		ADSR::Parameters implParams;
		implParams.attack = parameters.attack*globalMul;
		implParams.attackCurve = parameters.attackCurve;
		implParams.decay = parameters.decay*globalMul*velScaling; // Velocity can lengthen the decay phase
		implParams.decayCurve = parameters.decayCurve;
		implParams.sustain = parameters.sustain;
		implParams.release = parameters.release*globalMul;
		implParams.releaseCurve = parameters.releaseCurve;

		m_ADSR.setSampleRate(sampleRate);
		m_ADSR.setParameters(implParams);

		m_ADSR.noteOn();

		// Maarten van Strien advised this on account of his experience with FM8
		// It means the envelope will never go past it's sustain phase
		// This does however not apply if this envelope is part of a carrier operator!
		m_isInfinite = false == isCarrier && parameters.release == 1.f;
	}

	void Envelope::OnPianoSustain(float falloff, float pedalReleaseMul)
	{	
		SFM_ASSERT_NORM(falloff);
		SFM_ASSERT(pedalReleaseMul >= kPianoPedalMinReleaseMul && pedalReleaseMul <= kPianoPedalMaxReleaseMul);

		//
		// - Sustains at current output level
		// - Sustain falloff defines the length of the sustain phase based on the envelope's decay
		// - Release phase duration may be elongated by a parameter
		// 

		switch (m_ADSR.getState())
		{
		case ADSR::State::attack:
		case ADSR::State::decay:
		case ADSR::State::sustain:
			m_ADSR.pianoSustain(falloff);
			m_ADSR.scaleReleaseRate(pedalReleaseMul);
			break;

		case ADSR::State::pianosustain:
		case ADSR::State::release:
			break;

		default:
			SFM_ASSERT(false);
			break;
		}
	}
}
