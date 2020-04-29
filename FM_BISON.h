
/*
	MIT License

	Copyright (C) 2018- bipolaraudio.nl & visualizers.nl

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

/*
	FM. BISON hybrid FM synthesis
	(C) visualizers.nl & bipolaraudio.nl

	- Code is *very* verbose and has not been optimized yet (still in R&D stage)
	- Currently tooled to fit the JUCE 5.x framework
	- Synthesizer is stereo output only
	- Parameters are often filtered a bit, this is to alleviate crackle when for ex. coupled to a MIDI controller
	- 'SFM' is a legacy prefix & namespace name

	Third party credits (all of them public domain, please contact us if we're mistaken):
	- ADSR (modified), original by Nigel Redmon (earlevel.com)
	- SvfLinearTrapOptimised2.hpp (modified) by Fred Anton Corvest (https://github.com/FredAntonCorvest/Common-DSP)
	- MOOG-style ladder filter 'KrajeskiModel' (https://github.com/ddiakopoulos/MoogLadders/blob/master/src/)
	- Reverb based on FreeVerb by Volker Böhm
	- TinyMT by Makoto Matsumoto and Mutsuo Saito 
	- Research (DX7 LFO LUT) from Sean Bolton's Hexter
	- Fast cosine approximation by (or at least supplied by) Erik Faye-Lund
	- A few good bits of JUCE are used (though this has little priority, as we rely on JUCE for our products) (*)
	- Vowel (formant) filter: contribution to http://www.musicdsp.org by alex@smartelectronix.com
	- A few other sources were used; these are credited in or close to the implementation

	(*) - Depends on JUCE 5.4.7 or compatible!

	The following files belong to the "Helper" part of the codebase:
		- synth-aligned-alloc.h
		- synth-fast-cosine.*
		- synth-fast-tan.h
		- synth-helper.h
		- synth-log.*
		- synth-math.h
		- synth-ring-buffer.h

	Unused components:
		- RingBuffer
		- Vowelizer
	
	Core goals:
		- Yamaha DX7 style core FM with extensions
		- Subtractive synthesis on top
		- Low footprint use in DAWs & possibly embedded targets later

	VST/JUCE related:
		- See PluginProcessor.cpp
		- *This* library is currently not thread-safe (it does not have to be)

	Reading material:
		- https://www.hackaudio.com/digital-signal-processing/amplitude/peak-normalization/
		- https://cytomic.com/files/dsp/DynamicSmoothing.pdf
		- Your freshly bought Will Pirkle book
 
	Issues:
		- Inlining strategy a bit too agressive?
		- Class (object) design is just sloppy in places as I started with pure C
		- For embedded use this needs review, a lot of choices were made in favor of the VST plug-in
		- Analyze and optimize performance! (17/01/2020)

	Not supported on purpose:
		- Polyphonic aftertouch (or just anything MPE!)

	
	For issues & tasks please see Github repository.
*/

#pragma once

#include <deque>

#include "synth-global.h"

#include "synth-patch-global.h"
// #include "synth-one-pole-filters.h"
#include "synth-post-pass.h"
#include "synth-phase.h"
#include "synth-voice.h"

#include "synth-MIDI.h" // Purely for 2 constants

namespace SFM
{
	/*
		Interface tailored for JUCE VST plug-in
		Keep in mind this was never designed as a class but rather for embedded purposes, so it's not C++ 1-0-1
	*/

	class Bison
	{
	public:
		// Handles global initialization & release
		Bison();
		~Bison();

		// Called by JUCE's prepareToPlay()
		// Will stop all voices, reinitialize necessary objects and (re)set globals (see synth-globals.h)
		void OnSetSamplingProperties(unsigned sampleRate, unsigned samplesPerBlock);

		// Releases everything set by OnSetSamplingProperties()
		void DeleteRateDependentObjects();

		// Access to patch (or preset, if you will); never access during Render(), if so, fix by double-buffering
		Patch& GetPatch()
		{
			return m_patch;
		}

		void ResetVoices()
		{
			// Reset (i.e. quickly fade) all voices on next Render() call
			m_resetVoices = true;
		}

		void ResetPostPass()
		{
			if (nullptr != m_postPass)
			{
				// Create a new instance, that way we won't have to fiddle with details
				// However, do *not* call this often while rendering
				delete m_postPass;
				m_postPass = new PostPass(m_sampleRate, m_samplesPerBlock, m_Nyquist);
			}
		}
		
		// Render number of samples to 2 channels (stereo)
		// Bend wheel: amount of pitch bend (wheel) [-1..1]
		// Modulation: amount of modulation (wheel)  [0..1]
		// Aftertouch: amount of (monophonic) aftertouch
		void Render(unsigned numSamples, float bendWheel, float modulation, float aftertouch, float *pLeft, float *pRight);

		// Set BPM (can be used as LFO frequency)
		void SetBPM(double BPM, bool resetPhase)
		{
			m_resetPhaseBPM = resetPhase;

			if (m_BPM != BPM)
			{
				Log("Host has set new BPM: " + std::to_string(BPM));
				m_BPM = BPM;
			}
		}

		// Note events
		void NoteOn(unsigned key, float frequency, float velocity /* -1.f to use internal table */, unsigned timeStamp /* See VoiceRequest */, bool isMonoRetrigger = false /* Internal use only! */);
		void NoteOff(unsigned key, unsigned timeStamp);
		
		// Apply sustain to (active) voices
		void Sustain(bool state)
		{
			m_sustain = state;
		}

		unsigned GetSampleRate() const      { return m_sampleRate;      }
		unsigned GetSamplesPerBlock() const { return m_samplesPerBlock; }
		unsigned GetNyquist() const         { return m_Nyquist;         }

	private:
		/*
			Voice management
		*/

		bool m_resetVoices = false;

		struct VoiceRequest
		{
			unsigned key;       // [0..127] (MIDI)
			float frequency;    // Supplied by JUCE (FIXME)
			float velocity;     // [0..1]
			unsigned timeStamp; // In amount of samples relative to those passed to Render() call
			bool monoRetrigger; // Internal: is retrigger of note in monophonic sequence
		};

		typedef unsigned VoiceReleaseRequest; // Simply a MIDI key number

		// Remove voice index from key
		SFM_INLINE void FreeKey(unsigned key)
		{
			SFM_ASSERT(key <= 127);

			m_keyToVoice[key] = -1;
		}
	
		// Get voice index associated with key
		SFM_INLINE int GetVoice(unsigned key)
		{
			SFM_ASSERT(key <= 127);
			return m_keyToVoice[key];
		}

		// Associate voice index with *available* key
		SFM_INLINE void SetKey(unsigned key, int index)
		{
			SFM_ASSERT(key <= 127);
			SFM_ASSERT(index >= 0 && index < kMaxVoices);

			m_keyToVoice[key] = index;
		}

		void ReleaseVoice(int index); // Release voice (does *not* free key)
		void FreeVoice(int index);    // Free voice
		void StealVoice(int index);   // Steal voice

		float CalcOpFreq(float fundamentalFreq, const FM_Patch::Operator &patchOp);
		float CalcOpIndex(unsigned key, float velocity, const FM_Patch::Operator &patchOp);

		void InitializeVoice(const VoiceRequest &request, unsigned iVoice);
		void InitializeMonoVoice(const VoiceRequest &request);

		// Use front request to initialize new voice
		SFM_INLINE void InitializeVoice(unsigned iVoice)
		{
			const VoiceRequest &request = m_voiceReq.front();

			if (Patch::VoiceMode::kMono != m_curVoiceMode)
				InitializeVoice(request, iVoice);
			else
			{
				SFM_ASSERT(0 == iVoice);
				InitializeMonoVoice(request);
			}
			
			Log("Voice triggered: " + std::to_string(iVoice) + ", key: " + std::to_string(m_voices[iVoice].m_key));
		
			// Done: pop it!
			m_voiceReq.pop_front();
		}

		// Called by Render()
		void UpdateVoicesPreRender();
		void UpdateVoicesPostRender();
		void UpdateSustain();

		/*
			Vars.
		*/

		// Sample rate related (driven by JUCE)
		unsigned m_sampleRate;
		unsigned m_Nyquist;
		unsigned m_samplesPerBlock;

		// Parameters (patch)
		Patch m_patch;

		double m_BPM;         // Current BPM (if any)
		float m_freqBPM;      // Current BPM ratio-adjusted frequency (updated in Render())
		bool m_resetPhaseBPM; // Set if phase of BPM lock has to be reset

		// Current polyphony
		unsigned m_curPolyphony;

		// Cur. voice mode
		bool m_modeSwitch;
		Patch::VoiceMode m_curVoiceMode;

		// Monophonic state
		std::deque<VoiceRequest> m_monoReq;

		// Sustain?
		bool m_sustain;
	
		// Parameter filters
		ParameterFilter m_LFORatePF;
		ParameterFilter m_cutoffPF, m_resoPF;
		ParameterFilter m_effectWetPF, m_effectRatePF;
		ParameterFilter m_delayPF, m_delayWetPF, m_delayFeedbackPF, m_delayFeedbackCutoffPF;
		ParameterFilter m_postCutoffPF;
		ParameterFilter m_postResoPF;
		ParameterFilter m_postDrivePF;
		ParameterFilter m_postWetPF;
		ParameterFilter m_tubeDistPF;
		ParameterFilter m_avgVelocityPF;
		ParameterFilter m_reverbWetPF;
		ParameterFilter m_reverbRoomSizePF;
		ParameterFilter m_reverbDampeningPF;
		ParameterFilter m_reverbWidthPF;
		ParameterFilter m_reverbHP_PF;
		ParameterFilter m_reverbLP_PF;
		ParameterFilter m_reverbPreDelayPF;
		ParameterFilter m_compLookaheadPF;
		ParameterFilter m_masterVolPF;

		// Per-sample interpolated global parameters
		InterpolatedParameter<kLinInterpolate> m_curCutoff;
		InterpolatedParameter<kLinInterpolate> m_curQ;
		
		// Not in patch but supplied as parameters:
		ParameterFilter m_bendWheelPF;
		ParameterFilter m_modulationPF;
		ParameterFilter m_aftertouchPF;

		// Not in patch but supplied as parameters:
		InterpolatedParameter<kLinInterpolate> m_curPitchBend;
		InterpolatedParameter<kLinInterpolate> m_curAmpBend;
		InterpolatedParameter<kLinInterpolate> m_curModulation;
		InterpolatedParameter<kLinInterpolate> m_curAftertouch;
	
		// Effects
		PostPass *m_postPass = nullptr;

		// Running LFO (used for no key sync.)
		Phase *m_globalLFO = nullptr;

		// Necessary to reset filter on type switch
		SvfLinearTrapOptimised2::FLT_TYPE m_curFilterType; 

		// Intermediate buffers
		float *m_pBufL = nullptr;
		float *m_pBufR = nullptr;

		alignas(16) Voice m_voices[kMaxVoices];       // Array of voices to use
		alignas(16) bool  m_voicesStolen[kMaxVoices]; // Simple way to flag voices as stolen; contain related logic in FM_BISON.cpp

		// Global voice count
		unsigned m_voiceCount = 0;

		// Voice trigger & release requests
		// This may be redundant in certain scenarios but in a thread-safe situation these will come in handy
		std::deque<VoiceRequest> m_voiceReq;
		std::deque<VoiceReleaseRequest> m_voiceReleaseReq;

		// Key-to-voice mapping table
		int m_keyToVoice[128];
	};
}
 
