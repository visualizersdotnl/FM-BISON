
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
	- Currently fitted to play nice with the JUCE framework, needs significant work to work with other platforms (especially embeddedO
	- Synthesizer is stereo output *only*
	- Parameters are not just interpolated by also filtered to alleviate crackle during automation or MIDI control (see 'ParameterSlew')
	- 'SFM' is a legacy prefix & namespace name

	Third party credits (public domain, please contact us if we're mistaken):
	- ADSR & Biquad filter by Nigel Redmon (http://www.earlevel.com)
	- SvfLinearTrapOptimised2.hpp by Fred Anton Corvest (https://github.com/FredAntonCorvest/Common-DSP)
	- MOOG-style ladder filter: https://github.com/ddiakopoulos/MoogLadders/blob/master/src/
	- Butterworth 24dB filter found @ http://www.musicdsp.org (currently not in use, 05/08/2020)
	- Reverb based on FreeVerb by Volker Böhm
	- TinyMT Mersenne-Twister random generator by Makoto Matsumoto and Mutsuo Saito 
	- Yamaha DX7 LFO rates (synth-DX7-LFO-table.h) taken from Sean Bolton's Hexter
	- Fast cosine approximation supplied by Erik 'Kusma' Faye-Lund
	- There are 2 dependencies on JUCE (currently these have little priority as we use JUCE for our product line)
	- 'PolyBLEP'-based oscillators were lifted from https://github.com/martinfinke/PolyBLEP; by various authors (I keep a copy in /3rdparty/PolyBLEP (ref.))
	- I've ported a lot of interpolation functions from http://easings.net to single prec.
	- *Big* thank you Adam Szabo for his thesis on the JP-8000 supersaw: https://pdfs.semanticscholar.org/1852/250068e864215dd7f12755cf00636868a251.pdf 
	- A few bits and bytes left and right; these are all credited in or close to the implementation

	Most of these were modified and optimized by us

	** Depends on JUCE 6.01 or compatible! **
	
	Core goals:
		- Yamaha DX7 style core FM tone generator with a plethora of extra features
		- Subtractive synthesis (filters & effects) on top
		- Goal: low CPU footprint in DAWs, possibly embedded targets in the future

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

#include <thread>

#include "synth-global.h"

#include "patch/synth-patch-global.h"
#include "synth-post-pass.h"
#include "synth-phase.h"
#include "synth-voice.h"
#include "synth-sidechain-envelope.h"

namespace SFM
{
	// 'Structure was padded due to alignment specifier'
	#pragma warning (push)
	#pragma warning (disable: 4324)

	/*
		Interface tailored for JUCE VST plug-in
		Keep in mind this was never designed as a class but rather for embedded purposes, so it's not C++ 1-0-1
	*/

	class Bison
	{
	public:
		//
		// API
		//
		
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

		// This function can be called at any point whilst rendering
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
		void SetBPM(float BPM, bool resetPhase)
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
			if (nullptr != m_postPass)
				return m_postPass->GetCompressorBite();
			else
				return 0.f;
		}

		// Value is operator normalized peak (not affected by amplitude (output level) if modulator only)
		float GetOperatorPeak(unsigned iOp) const
		{
			SFM_ASSERT(iOp < kNumOperators);
			return m_opPeaks[iOp];
		}
		
		//
		// API
		//

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
		
		// Used by Initialize(Mono)Voice()
		void InitializeLFOs(Voice &voice, float jitter);

		// Voice initalization
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
		void UpdateVoicesPreRender();
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

		float m_BPM;          // Current BPM (if any)
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
	
		// Parameter slew (called each Render(), against artifacts; crackle, mostly)
		class ParameterSlew
		{
		public:
			ParameterSlew() {}

			ParameterSlew(unsigned sampleRate, float state = 0.f, float MS = kDefParameterSlewMS) :
				m_sigEnv(sampleRate, MS)
,				m_state(state)
			{
			}

			void Reset(float value)
			{
				m_state = value;
			}

			SFM_INLINE float Apply(float sample)
			{
				return m_sigEnv.Apply(sample, m_state);
			}

			SFM_INLINE float Get() const 
			{
				return m_state;
			}

		private:
			SignalFollower m_sigEnv;
			float m_state;
		};

		// FIXME: perhaps it would be nice to stash these in a map or a structure at least
		ParameterSlew m_LFORatePS;
		ParameterSlew m_LFOBlendPS;
		ParameterSlew m_LFOModDepthPS;
		ParameterSlew m_SandHSlewRatePS;
		ParameterSlew m_cutoffPS, m_resoPS;
		ParameterSlew m_effectWetPS, m_effectRatePS;
		ParameterSlew m_delayPS, m_delayWetPS, m_delayDrivePS, m_delayFeedbackPS, m_delayFeedbackCutoffPS, m_delayTapeWowPS;
		ParameterSlew m_postCutoffPS, m_postResoPS, m_postDrivePS, m_postWetPS;
		ParameterSlew m_tubeDistPS, m_tubeDrivePS;
		ParameterSlew m_wahRatePS, m_wahDrivePS, m_wahSpeakPS, m_wahSpeakVowelPS, m_wahSpeakVowelModPS, m_wahSpeakGhostPS, m_wahSpeakCutPS, m_wahSpeakResoPS, m_wahCutPS, m_wahWetPS;
		ParameterSlew m_reverbWetPS;
		ParameterSlew m_reverbRoomSizePS;
		ParameterSlew m_reverbDampeningPS;
		ParameterSlew m_reverbWidthPS;
		ParameterSlew m_reverbBassTuningdBPS;
		ParameterSlew m_reverbTrebleTuningdBPS;
		ParameterSlew m_reverbPreDelayPS;
		ParameterSlew m_compLookaheadPS;
		ParameterSlew m_masterVoldBPS;
		ParameterSlew m_bassTuningdBPS, m_trebleTuningdBPS;

		// Slew for auto-wah pedal mode (so it doesn't sound that harsh, I know this is unusual for auto-wah, but a lot of things about FM. BISON are unusual)
		ParameterSlew m_autoWahPedalPS;

		// Per-sample interpolated global parameters
		InterpolatedParameter<kLinInterpolate> m_curLFOBlend;
		InterpolatedParameter<kLinInterpolate> m_curLFOModDepth;
		InterpolatedParameter<kLinInterpolate> m_curCutoff;
		InterpolatedParameter<kLinInterpolate> m_curQ;
		
		// Not in patch but supplied as parameters:
		ParameterSlew m_bendWheelPS;
		ParameterSlew m_modulationPS; // Can be overridden by patch parameter
		ParameterSlew m_aftertouchPS;

		// Not in patch but supplied as parameters:
		InterpolatedParameter<kLinInterpolate> m_curPitchBend;
		InterpolatedParameter<kLinInterpolate> m_curAmpBend; // Gain
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

		// Per operator peaks
		SignalFollower m_opPeaksEnv[kNumOperators];
		float m_opPeaks[kNumOperators];
	};

	#pragma warning (pop)
}
