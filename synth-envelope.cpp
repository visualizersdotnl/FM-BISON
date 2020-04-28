
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

		// Nigel's sensible analog-style defaults
		const float sensibleRatioA = 0.3f;
		const float sensibleRatioDR = 0.0001f;
		
		// Linear ratio (give or take a tiny bit)
		const float nearLinearRatio = 1.f;

		// Blend ratios from linear to Nigel's analog-style synth. ratios
		// Smoothstepping the ratios gives a slightly better sense of control at the edges
		const float ratioA = lerpf<float>(nearLinearRatio, sensibleRatioA,  smoothstepf(parameters.attackCurve));
		const float ratioD = lerpf<float>(nearLinearRatio, sensibleRatioDR, smoothstepf(parameters.decayCurve));
		const float ratioR = lerpf<float>(nearLinearRatio, sensibleRatioDR, smoothstepf(parameters.releaseCurve));

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
		// On piano sustain pedal behaviour:
		// Source: Wikipedia
		//
		// "A sustain pedal or sustaining pedal (also called damper pedal, loud pedal, or open pedal[1]) is the most commonly used pedal in a modern piano. 
		//  It is typically the rightmost of two or three pedals. When pressed, the sustain pedal "sustains" all the damped strings on the piano by moving all the dampers away 
		//  from the strings and allowing them to vibrate freely. All notes played will continue to sound until the vibration naturally ceases, or until the pedal is released."

		// On sustain pedal channel pressure:
		// Source: KVR forum
		//
		// "As explained... the typical keyboard sustain pedal (CC#64) only outputs two values: on and off (0 and 127). Been like that forever. 
		//  What are called half-damper pedals are indeed available, but whether or the intermediate values they output will have any effect on the sound is entirely 
		//  dependent on how the sound is programmed, and/or whether or not the instrument you're playing is designed to respond to half-damper values 
		//  (which vary between 0 - 127). There's also a matter of your controller and whether or not it will interpret intermediate values of CC#64 at all.
		//
		//  The idea that the amount of pressure on a pedal influences the sustain on a real piano is limited to a very narrow range of effectiveness. 
		//  As you probably know, the piano pedal determines how far away from the strings the dampers are raised, but since the felt of the dampers are usually rather 
		//  compact, the amount that you can control the vibrations of the strings with the surface of the felts is very limited."

		//
		// Now what does this implementation do?
		// - Sustains at current output level unless attack phase is not finished yet
		// - Sustain has falloff (mimicking decaying strings, parameter)
		// - Release phase duration in multiplied (parameter)
		// - Release phase curve is set to be linear
		// - 2 parameters are available to "fit" patch
		//

		switch (m_ADSR.getState())
		{
		case ADSR::env_idle:
			break;

		case ADSR::env_attack:
		case ADSR::env_decay:
		case ADSR::env_sustain:
			{
				const double falloffRatio = (-1.0 == falloff) ? 0.0 : 0.4 - falloff*0.2; // 4000 to 2000 times Nigel's default; a fine range until some tester complains ;)
				m_ADSR.pianoSustain(sampleRate, falloffRatio);

				const double releaseRate = m_ADSR.getReleaseRate();
				m_ADSR.setReleaseRate(releaseRate*releaseRateMul);

				// *Very* linear release curve (FIXME: *could* be a parameter)
				m_ADSR.setTargetRatioR(100.f); 
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
