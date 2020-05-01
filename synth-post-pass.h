
/*
	FM. BISON hybrid FM synthesis -- Post-processing pass.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME:
		- Almost the entire path is implemented in Apply(), chop this up into smaller pieces?
		- Write own (or adapt public domain) up- and downsampling routines
		- The list of parameters is rather huge, pass through a structure?
*/

#pragma once

#include "3rdparty/SvfLinearTrapOptimised2.hpp"
#include "3rdparty/KrajeskiModel.h"

// Include JUCE (for up- and downsampling)
#include "../JuceLibraryCode/JuceHeader.h"

#include "synth-global.h"
#include "synth-delay-line.h"
#include "synth-phase.h"
#include "synth-one-pole-filters.h"
#include "synth-interpolated-parameter.h"
#include "synth-reverb.h"
#include "synth-compressor.h"
#include "synth-auto-wah.h"

namespace SFM
{
	const unsigned kNumPhaserStages = 7;

	class PostPass
	{
	public:
		PostPass(unsigned sampleRate, unsigned maxSamplesPerBlock, unsigned Nyquist);
		~PostPass();

		void Apply(unsigned numSamples,
		           float rateBPM, /* See impl. for details! */
				   float wahSlack, float wahAttack, float wahHold, float wahRate, float wahSpeak, float wahCut, float wahWet,
		           float cpRate, float cpWet, bool isChorus,
		           float delayInSec, float delayWet, float delayFeedback, float delayFeedbackCutoff,
		           float postCutoff, float postQ, float postDrivedB, float postWet,
		           float avgVelocity /* <- [0..1] */, float tubeDistort, float tubeDrivedB,
		           float reverbWet, float reverbRoomSize, float reverbDampening, float reverbWidth, float reverbLP, float reverbHP, float reverbPreDelay,
		           float compPeakToRMS, float compThresholddB, float compKneedB, float compRatio, float compGaindB, float compAttack, float compRelease, float compLookahead,
		           float masterVol,
		           const float *pLeftIn, const float *pRightIn, float *pLeftOut, float *pRightOut);

		unsigned GetOversamplingRate() const
		{
			return m_oversamplingRate;
		}

	private:
		SFM_INLINE void SetChorusRate(float rate, float scale) // [0..1]
		{
			rate *= scale;

			m_chorusSweep.SetFrequency(rate);

			// This is a happy little accident since SetPitch() expects a frequency but gets something
			// way smaller, which happens to sound good
			m_chorusSweepMod.SetFrequency(m_chorusSweep.GetPitch()*0.1);
		}

		SFM_INLINE void SetPhaserRate(float rate, float scale) // [0..1]
		{
			rate *= scale;

			m_phaserSweep.SetFrequency(rate);
		}
		
		void ApplyChorus(float sampleL, float sampleR, float &outL, float &outR, float wetness);
		void ApplyPhaser(float sampleL, float sampleR, float &outL, float &outR, float wetness);
		
		const unsigned m_sampleRate;
		const unsigned m_Nyquist;

		// Intermediate buffers
		float *m_pBufL = nullptr;
		float *m_pBufR = nullptr;

		// Delay lines
		DelayLine m_delayLineL;
		DelayLine m_delayLineM;
		DelayLine m_delayLineR;
		InterpolatedParameter<kLinInterpolate> m_curDelay;
		InterpolatedParameter<kLinInterpolate> m_curDelayWet;
		InterpolatedParameter<kLinInterpolate> m_curDelayFeedback;
		InterpolatedParameter<kLinInterpolate> m_curDelayFeedbackCutoff;
		
		// FIXME: enum. type
		int m_chorusOrPhaser;

		// Chorus
		DelayLine m_chorusDL;
		Phase m_chorusSweep, m_chorusSweepMod;
		LowpassFilter m_chorusSweepLPF1, m_chorusSweepLPF2;

		// Phaser
		SvfLinearTrapOptimised2 m_allpassFilters[kNumPhaserStages];
		Phase m_phaserSweep;
		LowpassFilter m_phaserSweepLPF;
		
		// Oversampling (JUCE)
		const unsigned m_oversamplingRate;
		juce::dsp::Oversampling<float> m_oversamplingL;
		juce::dsp::Oversampling<float> m_oversamplingR;

		// Post filter & it's interpolated parameters
		KrajeskiMoog m_postFilter;
		InterpolatedParameter<kLinInterpolate> m_curPostCutoff;
		InterpolatedParameter<kLinInterpolate> m_curPostQ;
		InterpolatedParameter<kLinInterpolate> m_curPostDrivedB;
		InterpolatedParameter<kLinInterpolate> m_curPostWet;

		// Tube amp. distortion post filter & it's interpolated parameters
		InterpolatedParameter<kLinInterpolate> m_curAvgVelocity;
		InterpolatedParameter<kLinInterpolate> m_curTubeDist;
		InterpolatedParameter<kLinInterpolate> m_curTubeDrive;
		SvfLinearTrapOptimised2 m_tubeFilterPre;
		SvfLinearTrapOptimised2 m_tubeFilterPost;
		
		// Low cut filter (DC blocker)
		LowBlocker m_lowCutFilter;
				
		// External effects
		AutoWah m_wah;
		Reverb m_reverb;
		Compressor m_compressor;

		// Misc.
		InterpolatedParameter<kLinInterpolate> m_curEffectWet;
		InterpolatedParameter<kLinInterpolate> m_curMasterVol;
	};
}
