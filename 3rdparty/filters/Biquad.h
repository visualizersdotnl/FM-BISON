
// ----------------------------------------------------------------------------------------------------
//
// Modified for FM. BISON
//
// This filter is the basic building block of filters; it does *not* respond well to rapid changes
// of the frequency, so much like the Butterworth 24dB it's mostly suited for specific tasks; that
// said, this filter is rather versatile (well suited for an EQ for example)
//
// ** Refrain from setting parameters one by one as they all call calcBiquad(), just use setBiquad()
//    to set up the filter as needed **
//
// ** Default process() call is now stereo, use processMono() for monaural, but do *not* mix and
//    match **
//
// - No external dependencies
// - Added reset() function
// - Ported to single precision
// - Added stereo support
// - Removed useless functions (like setQ() et cetera)
//
// Useful (graphical) design tool @ https://www.earlevel.com/main/2013/10/13/biquad-calculator-v2/
//
// I can't say I'm a big fan of how most DSP code is written but I'll try to keep it as-is.
//
// ----------------------------------------------------------------------------------------------------

#include "../../helper/synth-helper.h" // For SFM::kPI

//
//  Biquad.h
//
//  Created by Nigel Redmon on 11/24/12
//  EarLevel Engineering: earlevel.com
//  Copyright 2012 Nigel Redmon
//
//  For a complete explanation of the Biquad code:
//  http://www.earlevel.com/main/2012/11/26/biquad-c-source-code/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code
//  for your own purposes, free or commercial.
//

#ifndef Biquad_h
#define Biquad_h

#define _USE_MATH_DEFINES
#include <math.h>

#include "../../synth-global.h"

enum {
    bq_type_lowpass = 0,
    bq_type_highpass,
    bq_type_bandpass,
    bq_type_notch,
    bq_type_peak,
    bq_type_lowshelf,
    bq_type_highshelf
};

class Biquad {
public:
	Biquad()
	{
		reset();
	}

	Biquad(int type, float Fc, float Q, float peakGainDB)
	{
		setBiquad(type, Fc, Q, peakGainDB);

		z1l = z2l = 0.f; // Stereo (L)
		z1r = z2r = 0.f; // (R)
	}

	~Biquad() {}

	void reset();
	void setBiquad(int type, float Fc, float Q, float peakGaindB);

	SFM_INLINE void  process(float &sampleL, float &sampleR); 
	SFM_INLINE float processMono(float sample);

protected:
	void calcBiquad(void);

	// For BASS/TREBLE EQ
	SFM_INLINE void setLowShelf();
	SFM_INLINE void setHighShelf();

	int m_type;
	float m_Fc, m_Q, m_peakGain, m_FcK, m_peakGainV;

	float a0, a1, a2, b1, b2;
	float z1l, z2l, z1r, z2r;
};

SFM_INLINE void Biquad::process(float &sampleL, float &sampleR) { 
	const float outL = sampleL * a0 + z1l;
	z1l = sampleL * a1 + z2l - b1 * outL;
	z2l = sampleL * a2 - b2 * outL;
	
	const float outR = sampleR * a0 + z1r;
	z1r = sampleR * a1 + z2r - b1 * outR;
	z2r = sampleR * a2 - b2 * outR;

	sampleL = outL;
	sampleR = outR;
}

SFM_INLINE float Biquad::processMono(float sample) {
	const float out = sample * a0 + z1l; // Just use left Zs
	z1l = sample * a1 + z2l - b1 * out;  //
	z2l = sample * a2 - b2 * out;        //

	return out;
}

SFM_INLINE void Biquad::setBiquad(int type, float Fc, float Q, float peakGaindB)
{
	m_type = type;
	m_Q = Q;
	m_Fc = Fc;
	m_FcK = tanf(SFM::kPI*m_Fc);
	m_peakGain = peakGaindB;
	m_peakGainV = (0.f != m_peakGain) ? powf(10.f, fabsf(m_peakGain) / 20.f) : 1.f/20.f;
	
	switch (type)
	{
	case bq_type_lowshelf:
		setLowShelf();
		break;

	case bq_type_highshelf:
		setHighShelf();
		break;

	default:
		calcBiquad();
	};
}

SFM_INLINE void Biquad::setLowShelf()
{
	float norm;
	float V = m_peakGainV; // powf(10.f, fabsf(m_peakGain) / 20.f);
	float K = m_FcK;       // tanf(M_PI * m_Fc);

	if (m_peakGain >= 0.0) {    // boost
		norm = 1 / (1 + sqrtf(2) * K + K * K);
		a0 = (1 + sqrtf(2*V) * K + V * K * K) * norm;
		a1 = 2 * (V * K * K - 1) * norm;
		a2 = (1 - sqrtf(2*V) * K + V * K * K) * norm;
		b1 = 2 * (K * K - 1) * norm;
		b2 = (1 - sqrtf(2) * K + K * K) * norm;
	}
	else {    // cut
		norm = 1 / (1 + sqrtf(2*V) * K + V * K * K);
		a0 = (1 + sqrtf(2) * K + K * K) * norm;
		a1 = 2 * (K * K - 1) * norm;
		a2 = (1 - sqrtf(2) * K + K * K) * norm;
		b1 = 2 * (V * K * K - 1) * norm;
		b2 = (1 - sqrtf(2*V) * K + V * K * K) * norm;
	}
}

SFM_INLINE void Biquad::setHighShelf()
{
	float norm;
	float V = m_peakGainV; // powf(10.f, fabsf(m_peakGain) / 20.f);
	float K = m_FcK;       // tanf(M_PI * m_Fc);

	if (m_peakGain >= 0.0) {    // boost
		norm = 1 / (1 + sqrtf(2) * K + K * K);
		a0 = (V + sqrtf(2*V) * K + K * K) * norm;
		a1 = 2 * (K * K - V) * norm;
		a2 = (V - sqrtf(2*V) * K + K * K) * norm;
		b1 = 2 * (K * K - 1) * norm;
		b2 = (1 - sqrtf(2) * K + K * K) * norm;
	}
	else {    // cut
		norm = 1 / (V + sqrtf(2*V) * K + K * K);
		a0 = (1 + sqrtf(2) * K + K * K) * norm;
		a1 = 2 * (K * K - 1) * norm;
		a2 = (1 - sqrtf(2) * K + K * K) * norm;
		b1 = 2 * (K * K - V) * norm;
		b2 = (V - sqrtf(2*V) * K + K * K) * norm;
	}
}

#endif // Biquad_h
