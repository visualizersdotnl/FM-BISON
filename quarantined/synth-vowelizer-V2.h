
/*
	FM. BISON hybrid FM synthesis -- A vowel (formant) filter.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	We must get rid of this in favour of an actual formant filter as soon as possible!
	I've started writing one below the implementation of VowelizerV2.

	For new vocoder or formant filter if you will:
	- Ref.: https://www.youtube.com/watch?v=nPFzhwAJhGI
	- Process 2 signals: one to filter and follow, and a waveform to shape
*/

#pragma once

#include "../3rdparty/SvfLinearTrapOptimised2.hpp"

#include "../synth-global.h"
#include "../synth-sidechain-envelope.h"
#include "../synth-oscillator.h"

namespace SFM
{
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

	static const float kVowelFrequencies[kNumVowels][3] =
	{
		/* EE */ { 270.f, 2300.f, 3000.f },
		/* OO */ { 300.f,  870.f, 3000.f },
		/* I  */ { 400.f, 2000.f, 2250.f },
		/* E  */ { 530.f, 1850.f, 2500.f },
		/* U  */ { 640.f, 1200.f, 2400.f },
		/* A  */ { 660.f, 1700.f, 2400.f }
	};

	class VowelizerV2
	{
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
		
		WIP!
	*/

	constexpr unsigned kMaxFormantBands  = 32;

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
				m_analysisEnv[iBand].SetTimeCoeff(MS);
		}

		void Apply(float &left, float &right, float inputL, float inputR)
		{
			// Signal to be analyzed
			const float inL = inputL;
			const float inR = inputR;

			// Signal to be 'excited'
			const float carrierL = left;
			const float carrierR = right;
			
			// Accum. output
			float outL = 0.f;
			float outR = 0.f;

			// Process: filter analysis, gain calculation, filter carrier with gain applied, add to result
			for (unsigned iBand = 0; iBand < m_numBands; ++iBand)
			{
				float _inL = inL, _inR = inR;
				m_analysisBP[iBand].tick(_inL, _inR);
				
				// Calculate gain using envelope
//				const float bandGain = m_analysisEnv[iBand].Apply(GetRectifiedMaximum(_inL, _inR), m_envSig);
				const float bandGain = m_analysisEnv[iBand].Apply(_inL*0.5f + _inR*0.5f, m_envSig);

				// Filter carrier signal modulated by analysis gain
				float _carrierL = carrierL*bandGain, _carrierR = carrierR*bandGain; 
				m_synthesisBP[iBand].tick(_carrierL, _carrierR);

				// Apply gain to carrier and add (FIXME: gain adj.)
				outL += _carrierL;
				outR += _carrierR;
			}

			left  = outL;
			right = outR;
		}

	private:
		unsigned m_sampleRate;
		unsigned m_numBands;

		float m_bandWidthRatio = 1.f;

		void PrepareBands()
		{
			SFM_ASSERT(m_numBands > 0 && m_numBands <= kMaxFormantBands);
			SFM_ASSERT(m_bandWidthRatio > 0.f && m_bandWidthRatio <= 2.f);

			for (unsigned iBand = 1; iBand <= m_numBands; ++iBand)
			{
				m_analysisBP[iBand-1].resetState();
				m_synthesisBP[iBand-1].resetState();

				// Nicked from Romain Nichon's talk (he believes in a top freq. of 15541Hs)
				const float frequency = 25.f*powf(2.f, (iBand+1)*(9.f/m_numBands));
				const float bandWidth = (frequency - 25.f*powf(2.f, iBand*(9.f/m_numBands)))*m_bandWidthRatio;
				const float Q = 0.025f + std::min(39.5f, frequency/bandWidth);
				m_analysisBP[iBand-1].updateCoefficients(frequency, Q, SvfLinearTrapOptimised2::BAND_PASS_FILTER, m_sampleRate);
				m_synthesisBP[iBand-1].updateCoefficients(frequency, Q, SvfLinearTrapOptimised2::BAND_PASS_FILTER, m_sampleRate);
				
				// Set up followers
				m_analysisEnv[iBand-1].SetSampleRate(m_sampleRate);
				m_analysisEnv[iBand-1].SetTimeCoeff(1.f); // 1MS
			}
		}

		SvfLinearTrapOptimised2 m_analysisBP[kMaxFormantBands];
		SvfLinearTrapOptimised2 m_synthesisBP[kMaxFormantBands];

		SignalFollower m_analysisEnv[kMaxFormantBands];
		float m_envSig = 0.f;
	};
}
