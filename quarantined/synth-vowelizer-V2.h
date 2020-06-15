
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	We must get rid of this in favour of an actual formant filter as soon as possible!
	I've started writing one below the implementation of VowelizerV2.
*/

#pragma once

#include "../3rdparty/SvfLinearTrapOptimised2.hpp"

#include "../synth-global.h"
#include "../synth-sidechain-envelope.h"
#include "../synth-oscillator.h"

namespace SFM
{
	class VowelizerV2
	{
	public:
		enum Vowel
		{
			kEE,
			kOO,
			kI,
			kE,
			kU,
			kA,
			kNumVowels
		};

	public:
		VowelizerV2(unsigned sampleRate) :
			m_sampleRate(sampleRate)
		{
			Reset();
		}

		SFM_INLINE void Reset()
		{
			for (auto &filter : m_filterBP)
				filter.resetState();
		}

		void Apply(float &left, float &right, Vowel vowel);

	private:
		const unsigned m_sampleRate;

		SvfLinearTrapOptimised2 m_preFilter;
		SvfLinearTrapOptimised2 m_filterBP[3];		
	};

	/*
		Ref.: https://www.youtube.com/watch?v=nPFzhwAJhGI

		This should (become) a rather standard vocoder; the only real problem I have to tackle is that I don't
		really have any valid input to 'vocode' yet; I'm looking for a good sounding way to bend any input
		towards sounding like one out of a limited set of vowels.

		Idea: use the 3 frequencies in the Vowelizer_V2 table to feed 3 instances each operating at a
		      a sidechain signal (impulse train preferred) with *that* frequency?
	*/

	constexpr unsigned kMaxFormantBands  =   32;
	constexpr float kDefFormantAttackMS  = 0.1f; // 0.1MS
	constexpr float kDefFormantReleaseMS = 0.2f; // 0.2MS

	class FormantFilter
	{
	public:
		FormantFilter(unsigned sampleRate, unsigned numBands, float ratio = 1.f) :
			m_sampleRate(sampleRate)
,			m_numBands(numBands)
,			m_bandWidthRatio(ratio)
		{
			PrepareBands();
		}

		~FormantFilter() {}

		SFM_INLINE void SetAttack(float MS)
		{
			SFM_ASSERT(MS > 0.f);

			for (unsigned iBand = 0; iBand < m_numBands; ++iBand)
				m_analysisEnv[iBand].SetAttack(MS);
		}

		SFM_INLINE void SetRelease(float MS)
		{
			SFM_ASSERT(MS > 0.f);

			for (unsigned iBand = 0; iBand < m_numBands; ++iBand)
				m_analysisEnv[iBand].SetRelease(MS);
		}

		void Apply(float &left, float &right)
		{
			// FIXME: analysis (sidechain) input
			const float inL = left;
			const float inR = right;
			
			// Synth. input
			const float synthL = left;
			const float synthR = right;

			// Accum. output
			float outL = 0.f;
			float outR = 0.f;

			// Process bands
			for (unsigned iBand = 0; iBand < m_numBands; ++iBand)
			{
				// Filter analysis input
				float _inL = inL, _inR = inR;
				m_analysisBP[iBand].tick(_inL, _inR);
				
				// Calculate gain
				const float gain = m_analysisEnv[iBand].ApplyStereo(_inL, _inR);

				// Filter synth. input
				float _synthL = synthL, _synthR = synthR;
				m_synthesisBP[iBand].tick(_synthL, _synthR);

				// Apply gain to synth. input and add (FIXME: gain adj.)
				outL += _synthL*gain;
				outR += _synthR*gain;
			}

			left = outL;
			right = outR;
		}

	private:
		const unsigned m_sampleRate;
		const unsigned m_numBands;
		const float m_bandWidthRatio;

		void PrepareBands()
		{
			SFM_ASSERT(m_numBands > 0 && m_numBands < kMaxFormantBands);
			SFM_ASSERT(m_bandWidthRatio > 0.f && m_bandWidthRatio <= 2.f);

			for (unsigned iBand = 0; iBand < m_numBands; ++iBand)
			{
				// Nicked from Romain Nichon's talk
				const float frequency = 25.f*powf(2.f, (iBand+1)*(9.f/m_numBands));
				const float bandWidth = (frequency - 25.f*powf(2.f, iBand*(9.f/m_numBands)))*m_bandWidthRatio;
				const float Q = 0.025f + frequency/bandWidth;

				// FIXME: restrict to [0.025..40] range
				m_analysisBP[iBand].updateCoefficients(frequency, Q, SvfLinearTrapOptimised2::BAND_PASS_FILTER, m_sampleRate);
				m_synthesisBP[iBand].updateCoefficients(frequency, Q, SvfLinearTrapOptimised2::BAND_PASS_FILTER, m_sampleRate);

				m_analysisEnv[iBand].SetSampleRate(m_sampleRate);
				m_analysisEnv[iBand].SetAttack(kDefFormantAttackMS);
				m_analysisEnv[iBand].SetRelease(kDefFormantReleaseMS);
			}
		}

		SvfLinearTrapOptimised2 m_analysisBP[kMaxFormantBands];
		SvfLinearTrapOptimised2 m_synthesisBP[kMaxFormantBands];
		FollowerEnvelope m_analysisEnv[kMaxFormantBands];
	};
}
