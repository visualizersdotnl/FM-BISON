
//
// Modified to fit FM. BISON:
// - Shoved into SFM namespace (collided with JUCE's ADSR)
// - Separate target ratios for A/D/R
// - Made a few functions 'const'
// - Converted to double precision to avoid an attack "bug" (rounding artifact), as per Nigel Redmon's advice
// - Access to current release rate for SFM::Envelope::OnPianoSustain()
// - Extra state to finish attack prior to entering piano sustain phase (extra state so we don't need a boolean, not that it really matters though)
//

//
//  ADSR.h
//
//  Created by Nigel Redmon on 12/18/12.
//  EarLevel Engineering: earlevel.com
//  Copyright 2012 Nigel Redmon
//
//  For a complete explanation of the ADSR envelope generator and code,
//  read the series of articles by the author, starting here:
//  http://www.earlevel.com/main/2013/06/01/envelope-generators/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code for your own purposes, free or commercial.
//

#pragma once

#include "../synth-global.h"

namespace SFM 
{
	class ADSR {
	public:
		ADSR(void);
		~ADSR(void);
		float process(void);
		float getOutput(void) const;
		void sustain();
		void pianoSustain(unsigned sampleRate, double ratio);
		int getState(void) const;
		void gate(int on, double attackOutput);
		void setAttackRate(double rate);
		void setDecayRate(double rate);
		void setReleaseRate(double rate);
		void setSustainLevel(double level);
		void setTargetRatioA(double targetRatio);
		void setTargetRatioD(double targetRatio);
		void setTargetRatioR(double targetRatio);
		void reset(void);
		
		// Used by SFM::Envelope::OnPianoSustain()
		double getReleaseRate() const { return releaseRate; }

		enum envState {
			env_idle = 0,
			env_attack,
			env_decay,
			env_sustain,
			env_piano_attack,  // Used to let the attack finished prior to entering the state below
			env_piano_sustain, // Piano sustain, a sustain mode with (optional) falloff
			env_release,
		};

	protected:
		int state;
		double output;
		double attackRate;
		double decayRate;
		double releaseRate;
		double attackCoef;
		double decayCoef;
		double releaseCoef;
		double pianoSustainCoef;
		double sustainLevel;
		double targetRatioA;
		double targetRatioD;
		double targetRatioR;
		double attackBase;
		double decayBase;
		double releaseBase;
 
		double calcCoef(double rate, double targetRatio);
	};

	SFM_INLINE float ADSR::process() {
		switch (state) {
			case env_idle:
				output = 0.0;
				break;
			case env_attack:
				output = attackBase + output * attackCoef;
				if (output >= 1.0) {
					output = 1.0;
					state = env_decay;
				}
//				else break;
				break;
			case env_decay:
				output = decayBase + output * decayCoef;
				if (output <= sustainLevel) {
					output = sustainLevel;
					state = env_sustain;
				}
//				else break;
				break;
			case env_sustain:
				break;
			case env_piano_attack:
				output = attackBase + output * attackCoef;
				if (output >= 1.0) {
					output = 1.0;
					state = env_piano_sustain;
				}
				break;
			case env_piano_sustain:
				if (output > 0.0)
					output = output * pianoSustainCoef;
				break;
			case env_release:
				output = releaseBase + output * releaseCoef;
				if (output <= 0.0) {
					output = 0.0;
					state = env_idle;
				}
				break;
		}
		return float(output);
	}

	SFM_INLINE void ADSR::gate(int gate, double attackOutput) {
		if (gate)
		{
			state = env_attack;
			output = attackOutput;
		}
		else if (state != env_idle)
			state = env_release;
	}

	SFM_INLINE void ADSR::sustain() {
		// Sustain immediately
		sustainLevel = output; // Not strictly necessary
		state = env_sustain;
	}

	SFM_INLINE void ADSR::pianoSustain(unsigned sampleRate, double ratio) {
		// Sustain immediately with a slight decay
		pianoSustainCoef = calcCoef(double(sampleRate), ratio);
		sustainLevel = output; // Not strictly necessary
		state = env_piano_sustain;
	}

	SFM_INLINE int ADSR::getState() const {
		return state;
	}

	SFM_INLINE void ADSR::reset() {
		state = env_idle;
		output = 0.0;
	}

	SFM_INLINE float ADSR::getOutput() const {
		return float(output);
	}

} // SFM
