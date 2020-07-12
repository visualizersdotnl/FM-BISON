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
	a0 = 1.0;
	a1 = a2 = b1 = b2 = 0.0;
	m_Fc = 0.50;
	m_FcK = tan(M_PI*m_Fc);
	m_Q = 0.707;
	m_peakGain = 0.0;
	m_peakGainV = 1.0/20.0;
	z1 = z2 = 0.0;
}

void Biquad::setType(int type) {
	m_type = type;
	calcBiquad();
}

void Biquad::setQ(double Q) {
	m_Q = Q;
	calcBiquad();
}

void Biquad::setFc(double Fc) {
	m_Fc = Fc;
	m_FcK = tan(M_PI*m_Fc);
	calcBiquad();
}

void Biquad::setPeakGain(double peakGaindB) {
	m_peakGain = peakGaindB;
	m_peakGainV = (0.0 != m_peakGain) ? pow(10.0, fabs(m_peakGain) / 20.0) : 1.0/20.0;
	calcBiquad();
}
 
void Biquad::calcBiquad(void) {
	double norm;
	double V = m_peakGainV; // pow(10.0, fabs(m_peakGain) / 20.0);
	double K = m_FcK;       // tan(M_PI * m_Fc);
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
			if (m_peakGain >= 0.0) {    // boost
				norm = 1 / (1 + sqrt(2) * K + K * K);
				a0 = (1 + sqrt(2*V) * K + V * K * K) * norm;
				a1 = 2 * (V * K * K - 1) * norm;
				a2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
				b1 = 2 * (K * K - 1) * norm;
				b2 = (1 - sqrt(2) * K + K * K) * norm;
			}
			else {    // cut
				norm = 1 / (1 + sqrt(2*V) * K + V * K * K);
				a0 = (1 + sqrt(2) * K + K * K) * norm;
				a1 = 2 * (K * K - 1) * norm;
				a2 = (1 - sqrt(2) * K + K * K) * norm;
				b1 = 2 * (V * K * K - 1) * norm;
				b2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
			}
			break;

		case bq_type_highshelf:
			if (m_peakGain >= 0.0) {    // boost
				norm = 1 / (1 + sqrt(2) * K + K * K);
				a0 = (V + sqrt(2*V) * K + K * K) * norm;
				a1 = 2 * (K * K - V) * norm;
				a2 = (V - sqrt(2*V) * K + K * K) * norm;
				b1 = 2 * (K * K - 1) * norm;
				b2 = (1 - sqrt(2) * K + K * K) * norm;
			}
			else {    // cut
				norm = 1 / (V + sqrt(2*V) * K + K * K);
				a0 = (1 + sqrt(2) * K + K * K) * norm;
				a1 = 2 * (K * K - 1) * norm;
				a2 = (1 - sqrt(2) * K + K * K) * norm;
				b1 = 2 * (K * K - V) * norm;
				b2 = (V - sqrt(2*V) * K + K * K) * norm;
			}
			break;
	}
    
    return;
}
