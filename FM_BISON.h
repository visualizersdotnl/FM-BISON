
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

	- Code is quite verbose and has not been optimized "to the bone" yet (we're still in R&D stage)
	- Currently fitted to play nice with the JUCE framework
	- Synthesizer is stereo output *only*
	- Parameters are not just interpolated by also filtered to alleviate crackle during automation or MIDI control (see 'ParameterFilter')
	- 'SFM' is a legacy prefix & namespace name

	Third party credits (all of them public domain, please contact us if we're mistaken):
	- ADSR (modified & fixed) by Nigel Redmon (http://www.earlevel.com)
	- SvfLinearTrapOptimised2.hpp (modified) by Fred Anton Corvest (https://github.com/FredAntonCorvest/Common-DSP)
	- MOOG-style ladder filter 'KrajeskiModel' (https://github.com/ddiakopoulos/MoogLadders/blob/master/src/)
	- Reverb based on FreeVerb by Volker Böhm
	- TinyMT Mersenne-Twister random generator by Makoto Matsumoto and Mutsuo Saito 
	- Yamaha DX7 LFO rates taken from Sean Bolton's Hexter
	- Fast cosine approximation supplied by Erik 'Kusma' Faye-Lund
	- There are 2 depndencies on JUCE (currently these have little priority as we use JUCE for our product line)
	- 'PolyBLEP'-based oscillators were lifted from https://github.com/martinfinke/PolyBLEP; by various authors (I keep a copy in /3rdparty as ref.)
	- And a few bits and bytes left and right; these are often credited in or close to the implementation

	Depends on JUCE 5.4.7 or compatible!

	The following files belong to the 'helper' part of the codebase (included by synth-global.h):
		- synth-aligned-alloc.h
		- synth-fast-cosine.*
		- synth-fast-tan.h
		- synth-helper.h
		- synth-log.*
		- synth-math.h
		- synth-ring-buffer.h
		- synth-random.*
	
	Core goals:
		- Yamaha DX7 style core FM with extensions
		- Subtractive synthesis on top
		- Low CPU footprint in DAWs, possibly embedded targets in the future

	VST/JUCE related:
		- See PluginProcessor.cpp
		- This library is *not* thread-safe (does not have to be) though it uses them internally
 
	Issues:
		- I've spotted some potentially overzealous and inconsistent use of SFM_INLINE (29/05/2020)
		- Class (object) design is somewhwere inbetween C and C++; this is because the project started out
		  as a simple experiment; I've also started to use more modern C++ features here and there, but 
		  I'm not religious about it
		- Performance needs to be analyzed closer, especially on OSX

	Optimal compiler parameters for Visual Studio (MSVC):
		- /Ob2 /Oi /O2 /Ot

	
	For issues & tasks please see Github repository.
*/

#pragma once

// C++
#include <deque>
#include <thread>

#include "synth-global.h"

#include "synth-patch-global.h"
// #include "synth-one-pole-filters.h"
#include "synth-post-pass.h"
#include "synth-phase.h"
#include "synth-voice.h"
#include "synth-followers.h"

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

		// Note events (just to be sure: do *not* call these from different threads!)
		void NoteOn(
			unsigned key, 
			float frequency,               // Uses internal table if -1.f
			float velocity,                // Zero will *not* yield NOTE_OFF, handle that yourself
			unsigned timeStamp,            // See VoiceRequest 
			bool isMonoRetrigger = false); // Internal use only!

		void NoteOff(unsigned key, unsigned timeStamp);
		
		// Apply sustain to (active) voices
		void Sustain(bool state)
		{
			m_sustain = state;
		}

		unsigned GetSampleRate() const      { return m_sampleRate;      }
		unsigned GetSamplesPerBlock() const { return m_samplesPerBlock; }
		unsigned GetNyquist() const         { return m_Nyquist;         }
		
		// Value can be used to visually represent compressor "bite" (when RMS falls below threshold dB)
		float GetCompressorBite() const
		{
			SFM_ASSERT(nullptr != m_postPass);
			return m_postPass->GetCompressorBite();
		}

		// Value is operator normalized RMS
		float GetOperatorRMS(unsigned iOp) const
		{
			SFM_ASSERT(iOp < kNumOperators);
			const float RMS = m_opRMS[iOp].Get();
			return RMS;
		}

	private:

		/*
			Voice management
		*/

		bool m_resetVoices = false;

		struct VoiceRequest
		{
			unsigned key;       // [0..127] (MIDI)
			float frequency;    // By JUCE or internal table
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

		void SetOperatorFilters(unsigned key, SvfLinearTrapOptimised2 *filterSVF, SvfLinearTrapOptimised2 &modFilter, const PatchOperators::Operator &patchOp);
		float CalcOpFreq(float fundamentalFreq, float detuneOffs, const PatchOperators::Operator &patchOp);
		float CalcOpIndex(unsigned key, float velocity, const PatchOperators::Operator &patchOp);
		void InitializeLFO(Voice &voice, float jitter);

		void InitializeVoice(const VoiceRequest &request, unsigned iVoice);
		void InitializeMonoVoice(const VoiceRequest &request);

		// Use front (latest) request (list has been sorted in polyphonic mode) to initialize new voice
		SFM_INLINE void InitializeVoice(unsigned iVoice)
		{
			SFM_ASSERT(m_voiceReq.size() > 0);

			const VoiceRequest request = m_voiceReq.front();

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
		void UpdateVoicesPreRender(unsigned numSamples);
		void UpdateVoicesPostRender();
		void UpdateSustain();

		// Parameters for each voice to be rendered
		struct VoiceRenderParameters
		{
			float freqLFO;
			
			// Filter setup
			SvfLinearTrapOptimised2::FLT_TYPE filterType1;
			SvfLinearTrapOptimised2::FLT_TYPE filterType2;
			bool resetFilter;
			float qDiv;
			bool secondFilterPass;
			float secondQOffs;
			float fullCutoff;
			
			// Questionable cycle savers (FIXME)
			float modulationAftertouch;
			float mainFilterAftertouch;
		};

		// Voice thread basics (parameters, indices, buffers)
		struct VoiceThreadContext
		{
			VoiceThreadContext(const VoiceRenderParameters &parameters) :
				parameters(parameters) {}

			const VoiceRenderParameters &parameters;
			
			std::vector<unsigned> voiceIndices;

			unsigned numSamples = 0;
			float *pDestL = nullptr;
			float *pDestR = nullptr;
		};

		static void VoiceRenderThread(Bison *pInst, VoiceThreadContext *pContext);
		void RenderVoices(const VoiceRenderParameters &context, const std::vector<unsigned> &voiceIndices, unsigned numSamples, float *pDestL, float *pDestR) const;

		/*
			Variables.
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
	
		// Parameter followers (basically slew, against crackle)
		class ParameterFilter
		{
		public:
			ParameterFilter() {}

			ParameterFilter(unsigned sampleRate, float MS = kDefParameterFilterMS) :
				m_sigEnv(sampleRate, MS)
			{
			}

			void Reset(float value)
			{
				m_state = value;
			}

			float Apply(float sample)
			{
				return m_sigEnv.Apply(sample, m_state);
			}

			float Get() const 
			{
				return m_state;
			}

		private:
			SignalFollower m_sigEnv;
			float m_state = 0.f;
		};

		ParameterFilter m_LFORatePF;
		ParameterFilter m_LFOBlendPF;
		ParameterFilter m_LFOModDepthPF;
		ParameterFilter m_SandHSlewRatePF;
		ParameterFilter m_cutoffPF, m_resoPF;
		ParameterFilter m_effectWetPF, m_effectRatePF;
		ParameterFilter m_delayPF, m_delayWetPF, m_delayFeedbackPF, m_delayFeedbackCutoffPF;
		ParameterFilter m_postCutoffPF;
		ParameterFilter m_postResoPF;
		ParameterFilter m_postDrivePF;
		ParameterFilter m_postWetPF;
		ParameterFilter m_tubeDistPF, m_tubeDrivePF;
		ParameterFilter m_wahRatePF, m_wahSpeakPF, m_wahCutPF, m_wahWetPF;
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
		InterpolatedParameter<kLinInterpolate> m_curLFOBlend;
		InterpolatedParameter<kLinInterpolate> m_curLFOModDepth;
		InterpolatedParameter<kLinInterpolate> m_curCutoff;
		InterpolatedParameter<kLinInterpolate> m_curQ;
		
		// Not in patch but supplied as parameters:
		ParameterFilter m_bendWheelPF;
		ParameterFilter m_modulationPF; // Can be overridden by patch parameter
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
		float *m_pBufL[2] = { nullptr, nullptr };
		float *m_pBufR[2] = { nullptr, nullptr };

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

		// Per operator RMS (filtered)
		LowpassFilter12dB m_opRMS[kNumOperators];
	};
}
