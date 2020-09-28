
// Modified to fit FM. BISON (see Biquad.h)

//
//  Biquad.cpp
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

#define _USE_MATH_DEFINES
#include <math.h>

#include "Biquad.h"

void Biquad::reset()
{
	m_type = bq_type_lowpass;
	a0 = 1.f;
	a1 = a2 = b1 = b2 = 0.f;
	m_Fc = 0.5f;
	m_FcK = tanf(SFM::kPI*m_Fc);
	m_Q = 0.707f;
	m_peakGain = 0.f;
	m_peakGainV = 1.f/20.f;
	
	z1l = z2l = 0.f;
	z1r = z2r = 0.f;
}
 
void Biquad::calcBiquad(void) {
	float norm;
	float V = m_peakGainV; // powf(10.f, fabsf(m_peakGain) / 20.f);
	float K = m_FcK;       // tanf(M_PI * m_Fc);
	switch (m_type) {
		case bq_type_lowpass:
			norm = 1 / (1 + K / m_Q + K * K);
			a0 = K * K * norm;
			a1 = 2 * a0;
			a2 = a0;
			b1 = 2 * (K * K - 1) * norm;
			b2 = (1 - K / m_Q + K * K) * norm;
			break;
            
		case bq_type_highpass:
			norm = 1 / (1 + K / m_Q + K * K);
			a0 = 1 * norm;
			a1 = -2 * a0;
			a2 = a0;
			b1 = 2 * (K * K - 1) * norm;
			b2 = (1 - K / m_Q + K * K) * norm;
			break;
            
		case bq_type_bandpass:
			norm = 1 / (1 + K / m_Q + K * K);
			a0 = K / m_Q * norm;
			a1 = 0;
			a2 = -a0;
			b1 = 2 * (K * K - 1) * norm;
			b2 = (1 - K / m_Q + K * K) * norm;
			break;
            
		case bq_type_notch:
			norm = 1 / (1 + K / m_Q + K * K);
			a0 = (1 + K * K) * norm;
			a1 = 2 * (K * K - 1) * norm;
			a2 = a0;
			b1 = a1;
			b2 = (1 - K / m_Q + K * K) * norm;
			break;
            
		case bq_type_peak:
			if (m_peakGain >= 0.0) {    // boost
				norm = 1 / (1 + 1/m_Q * K + K * K);
				a0 = (1 + V/m_Q * K + K * K) * norm;
				a1 = 2 * (K * K - 1) * norm;
				a2 = (1 - V/m_Q * K + K * K) * norm;
				b1 = a1;
				b2 = (1 - 1/m_Q * K + K * K) * norm;
			}
			else {    // cut
				norm = 1 / (1 + V/m_Q * K + K * K);
				a0 = (1 + 1/m_Q * K + K * K) * norm;
				a1 = 2 * (K * K - 1) * norm;
				a2 = (1 - 1/m_Q * K + K * K) * norm;
				b1 = a1;
				b2 = (1 - V/m_Q * K + K * K) * norm;
			}
			break;

		case bq_type_lowshelf:
			if (m_peakGain >= 0.f) {    // boost
				norm = 1 / (1 + sqrtf(2) * K + K * K);
				a0 = (1 + sqrtf(2*V) * K + V * K * K) * norm;
				a1 = 2 * (V * K * K - 1) * norm;
				a2 = (1 - sqrtf(2*V) * K + V * K * K) * norm;
				b1 = 2 * (K * K - 1) * norm;
				b2 = (1 - sqrtf(2) * K + K * K) * norm;
			}
			else {    // cut
				norm = 1 / (1 + sqrt(2*V) * K + V * K * K);
				a0 = (1 + sqrtf(2) * K + K * K) * norm;
				a1 = 2 * (K * K - 1) * norm;
				a2 = (1 - sqrtf(2) * K + K * K) * norm;
				b1 = 2 * (V * K * K - 1) * norm;
				b2 = (1 - sqrtf(2*V) * K + V * K * K) * norm;
			}
			break;

		case bq_type_highshelf:
			if (m_peakGain >= 0.f) {    // boost
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
			break;
	}
}
