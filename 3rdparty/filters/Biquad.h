
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
// - No external dependencies
// - Added reset() function
// - Added support for double precision samples
// - Misc. modifications, fixes & optimizations (including a horrendous single precision impl.)
// - Stereo support (single prec. only)
// - Useful: https://www.earlevel.com/main/2013/10/13/biquad-calculator-v2/
//
// FIXME: just convert entire class to single precision already?
//
// I can't say I'm a big fan of how most DSP code is written but I'll try to keep it as-is.
//
// ----------------------------------------------------------------------------------------------------

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

	Biquad(int type, double Fc, double Q, double peakGainDB)
	{
		setBiquad(type, Fc, Q, peakGainDB);

		z1 = z2 = 0.0;     // Double prec.
		z1f = z2f = 0.f;   // Single prec.
		z1fs = z2fs = 0.f; // Single prec. stereo (R)
	}

	~Biquad() {}

	void reset();

	// Avoid calling these separately (encouraged by *not* inlining them)
	void setType(int type);
	void setQ(double Q);
	void setFc(double Fc);
	void setPeakGain(double peakGaindB);

	void setBiquad(int type, double Fc, double Q, double peakGaindB);

	SFM_INLINE double process(double in);                        // Mono, double prec.
	SFM_INLINE float  processf(float in);                        // Mono, single prec.
	SFM_INLINE void   processfs(float &sampleL, float &sampleR); // Stereo, single prec.

protected:
	SFM_INLINE void calcBiquad(void);

	int m_type;
	double m_Fc, m_Q, m_peakGain, m_FcK, m_peakGainV;

	double a0, a1, a2, b1, b2;
	double z1, z2;

	// This is the quickest and dirtiest way to add 100% single precision processing (omitting costly scalar conversion instructions)
	float a0f, a1f, a2f, b1f, b2f;
	float z1f, z2f;
	float z1fs, z2fs; // Stereo (R)
};

// Double prec. monaural
SFM_INLINE double Biquad::process(double in) {
	const double out = in * a0 + z1;
	z1 = in * a1 + z2 - b1 * out;
	z2 = in * a2 - b2 * out;
	return out;
}

// Single prec. monaural
SFM_INLINE float Biquad::processf(float in) { 
	const float out = in * a0f + z1f;
	z1f = in * a1f + z2f - b1f * out;
	z2f = in * a2f - b2f * out;
	return out;
}

// Single prec. stereo
SFM_INLINE void Biquad::processfs(float &sampleL, float &sampleR) { 
	const float outL = sampleL * a0f + z1f;
	z1f = sampleL * a1f + z2f - b1f * outL;
	z2f = sampleL * a2f - b2f * outL;
	
	// I could go the fancy route and call processf() twice, but why bother?
	const float outR = sampleR * a0f + z1fs;
	z1fs = sampleR * a1f + z2fs - b1f * outR;
	z2fs = sampleR * a2f - b2f * outR;

	sampleL = outL;
	sampleR = outR;
}

SFM_INLINE void Biquad::setBiquad(int type, double Fc, double Q, double peakGaindB)
{
	m_type = type;
	m_Q = Q;
	m_Fc = Fc;
	m_FcK = tan(M_PI*m_Fc);
	m_peakGain = peakGaindB;
	m_peakGainV = (0.0 != m_peakGain) ? pow(10.0, fabs(m_peakGain) / 20.0) : 1.0/20.0;
	
	calcBiquad();
}

#endif // Biquad_h
