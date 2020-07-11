
// ----------------------------------------------------------------------------------------------------
//
// Modified for FM. BISON
//
// This is an ideal filter for steep cutoffs without ripple
//
// - No external dependencies
// - Misc. minor modifications 
// - FIXME: needs stereo support
// - FIXME: needs optimization
//
// I can't say I'm a big fan of how most DSP code is written but I'll try to keep it as-is.
//
// Source: https://www.musicdsp.org/en/latest/Filters/243-butterworth-optimized-c-class.html
//
// ----------------------------------------------------------------------------------------------------

#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

#include "../../synth-global.h"

static constexpr float BUDDA_Q_SCALE = 6.0;

class Butterworth24dB
{
public:
    Butterworth24dB()
	{
		Reset();
	}

    ~Butterworth24dB() {}

	void Reset()
	{
		m_history1 = 0.0;
		m_history2 = 0.0;
		m_history3 = 0.0;
		m_history4 = 0.0;
	}

    void SetSampleRate(unsigned fs)
	{
		double pi = 4.0 * atan(1.0);

		m_t0 = 4.0 * fs*fs;
		m_t1 = 8.0 * fs*fs;
		m_t2 = 2.0 * fs;
		m_t3 = pi / fs;

		m_min_cutoff = fs * 0.01; // 0.01;
		m_max_cutoff = fs *  0.5; // 0.45;
	}

    void Set(double cutoff, double q)
	{
		if (cutoff < m_min_cutoff)
			cutoff = m_min_cutoff;
		else if(cutoff > m_max_cutoff)
			cutoff = m_max_cutoff;

		if(q < 0.f)
			q = 0.f;
		else if(q > 1.f)
			q = 1.f;

		double wp = m_t2 * tan(m_t3*cutoff);
		double bd, bd_tmp, b1, b2;

		q *= BUDDA_Q_SCALE;
		q += 1.0;

		b1 = (0.765367 / q) / wp;
		b2 = 1.0 / (wp * wp);

		bd_tmp = m_t0 * b2 + 1.0;

		bd = 1.0 / (bd_tmp + m_t2 * b1);

		m_gain = bd; // * 0.5

		m_coef2 = (2.0 - m_t1*b2);

		m_coef0 = m_coef2*bd;
		m_coef1 = (bd_tmp - m_t2*b1) * bd;

		b1 = (1.847759 / q) / wp;

		bd = 1.0 / (bd_tmp + m_t2*b1);

		m_gain *= bd;
		m_coef2 *= bd;
		m_coef3 = (bd_tmp - m_t2*b1) * bd;
	}

    float Run(float input)
	{
		double output = input*m_gain;
		double new_hist;

		output -= m_history1*m_coef0;
		new_hist = output - m_history2*m_coef1;

		output = new_hist + m_history1*2.0;
		output += m_history2;

		m_history2 = m_history1;
		m_history1 = new_hist;

		output -= m_history3*m_coef2;
		new_hist = output - m_history4*m_coef3;

		output = new_hist + m_history3*2.0;
		output += m_history4;

		m_history4 = m_history3;
		m_history3 = new_hist;

		return (float) output;
	}

private:
    double m_t0, m_t1, m_t2, m_t3;
    double m_coef0, m_coef1, m_coef2, m_coef3;
    double m_history1, m_history2, m_history3, m_history4;
    double m_gain;
    double m_min_cutoff, m_max_cutoff;
};
