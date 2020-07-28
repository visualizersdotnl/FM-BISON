
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
// - Misc. modifications, fixes & optimizations
// - Useful: https://www.earlevel.com/main/2013/10/13/biquad-calculator-v2/
//
// FIXME: 
// - Stereo support
// - Just convert to single precision?
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
		z1 = z2 = 0.0;
	}

	~Biquad() {}

	void reset();

	// Avoid calling these separately (encouraged by *not* inlining them)
	void setType(int type);
	void setQ(double Q);
	void setFc(double Fc);
	void setPeakGain(double peakGaindB);

	void setBiquad(int type, double Fc, double Q, double peakGaindB);

	SFM_INLINE double process(double in);
	SFM_INLINE float  processf(float in);
    
protected:
	SFM_INLINE void calcBiquad(void);
	SFM_INLINE void calcBiquadHPF(void);

	int m_type;
	double m_Fc, m_Q, m_peakGain, m_FcK, m_peakGainV;

	double a0, a1, a2, b1, b2;
	double z1, z2;
};

SFM_INLINE double Biquad::process(double in) {
	double out = in * a0 + z1;
	z1 = in * a1 + z2 - b1 * out;
	z2 = in * a2 - b2 * out;
	return out;
}

SFM_INLINE float Biquad::processf(float in) {
	double out = in * a0 + z1;
	z1 = in * a1 + z2 - b1 * out;
	z2 = in * a2 - b2 * out;
	return float(out);
}

SFM_INLINE void Biquad::setBiquad(int type, double Fc, double Q, double peakGaindB)
{
	m_type = type;
	m_Q = Q;
	m_Fc = Fc;
	m_FcK = tan(M_PI*m_Fc);
	m_peakGain = peakGaindB;
	m_peakGainV = (0.0 != m_peakGain) ? pow(10.0, fabs(m_peakGain) / 20.0) : 1.0/20.0;
	
	if (bq_type_highpass == m_type)
		calcBiquadHPF();
	else
		calcBiquad();
}

// FIXME: this was once useful, move it back into Biquad.cpp?
SFM_INLINE void Biquad::calcBiquadHPF()
{
	SFM_ASSERT(bq_type_highpass == m_type);

	double norm;
	double K = m_FcK; // tan(M_PI * m_Fc);

	norm = 1 / (1 + K / m_Q + K * K);
	a0 = 1 * norm;
	a1 = -2 * a0;
	a2 = a0;
	b1 = 2 * (K * K - 1) * norm;
	b2 = (1 - K / m_Q + K * K) * norm;
 }

#endif // Biquad_h
