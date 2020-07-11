
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

#include "3rdparty/filters/SvfLinearTrapOptimised2.hpp"
#include "3rdparty/filters/MusicDSPModel.h"
// #include "3rdparty/filters/Butterworth24dB.h"
#include "3rdparty/filters/Biquad.h"

// Include JUCE (for juce::dsp::Oversampling)
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
	const unsigned kNumPhaserStages = 8;

	class PostPass
	{
	public:
		PostPass(unsigned sampleRate, unsigned maxSamplesPerBlock, unsigned Nyquist);
		~PostPass();

		// FIXME: this parameter list is just too ridiculously long!
		void Apply(unsigned numSamples,
		           float rateBPM, /* See impl. for details! */
				   float wahResonance, float wahAttack, float wahHold, float wahRate, float wahDrivedB, float wahSpeak, float wahSpeakVowel, float wahSpeakVowelMod, float wahSpeakGhost, float wahSpeakCut, float wahSpeakReso, float wahCut, float wahWet,
		           float cpRate, float cpWet, bool isChorus,
		           float delayInSec, float delayWet, float delayFeedback, float delayFeedbackCutoff,
		           float postCutoff, float postReso, float postDrivedB, float postWet,
		           float tubeDistort, float tubeDrive, float tubeOffset,
				   float antiAliasing,
		           float reverbWet, float reverbRoomSize, float reverbDampening, float reverbWidth, float reverbLP, float reverbHP, float reverbPreDelay,
		           float compThresholddB, float compKneedB, float compRatio, float compGaindB, float compAttack, float compRelease, float compLookahead, bool compAutoGain, float compRMSToPeak,
		           float masterVol,
		           const float *pLeftIn, const float *pRightIn, float *pLeftOut, float *pRightOut);

		// Intended for a graphical indicator
		float GetCompressorBite() const
		{
			return m_compressorBiteLPF.Get();
		}

	private:
		SFM_INLINE void SetChorusRate(float rate, float scale) // [0..1]
		{
			rate *= scale;

			m_chorusSweep.SetFrequency(rate);

			// This is a happy little accident since SetFrequency() expects a frequency but
			// gets a 10th of the pitch instead; but it sounds good so I'm not messing with this
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
		const unsigned m_sampleRate4X; // Convenience

		// Intermediate buffers
		float *m_pBufL = nullptr;
		float *m_pBufR = nullptr;

		// Delay lines & delay's interpolated parameters
		DelayLine m_delayLineL;
		DelayLine m_delayLineM;
		DelayLine m_delayLineR;
		LowpassFilter12dB m_delayFeedbackLPF_L, m_delayFeedbackLPF_R;
		InterpolatedParameter<kLinInterpolate> m_curDelayInSec;
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

		// Oversampling (JUCE, FIXME)
		juce::dsp::Oversampling<float> m_oversampling4X;

		// Post filter & interpolated parameters
		MusicDSPMoog m_postFilter;
		InterpolatedParameter<kLinInterpolate> m_curPostCutoff;
		InterpolatedParameter<kLinInterpolate> m_curPostReso;
		InterpolatedParameter<kLinInterpolate> m_curPostDrive;
		InterpolatedParameter<kLinInterpolate> m_curPostWet;

		// Tube distortion filter (AA), DC blocker & interpolated parameters
		InterpolatedParameter<kLinInterpolate> m_curTubeDist;
		InterpolatedParameter<kLinInterpolate> m_curTubeDrive;
		InterpolatedParameter<kLinInterpolate> m_curTubeOffset;
		StereoDCBlocker m_tubeDCBlocker;	
		Biquad m_tubeFilterAA_L, m_tubeFilterAA_R;
		Biquad m_finalFilterAA_L, m_finalFilterAA_R;
		
		// Low & DC blocker
		LowBlocker m_lowBlocker;
		StereoDCBlocker m_DCBlocker;
				
		// External effects
		AutoWah m_wah;
		Reverb m_reverb;
		Compressor m_compressor;

		// Exposed to be used, chiefly, as indicator
		LowpassFilter12dB m_compressorBiteLPF;

		// Misc.
		InterpolatedParameter<kLinInterpolate> m_curEffectWet;
		InterpolatedParameter<kMulInterpolate> m_curMasterVoldB;
	};
}
