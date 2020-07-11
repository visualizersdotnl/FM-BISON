
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
// - Misc. modifications
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
    Biquad();
    Biquad(int type, double Fc, double Q, double peakGainDB);
    ~Biquad();
	void reset();
    void setType(int type);
    void setQ(double Q);
    void setFc(double Fc);
    void setPeakGain(double peakGainDB);
    void setBiquad(int type, double Fc, double Q, double peakGain);
    float process(float in);
    
protected:
    void calcBiquad(void);

    int m_type;
    double m_Fc, m_Q, m_peakGain;

    double a0, a1, a2, b1, b2;
    double z1, z2;
};

inline float Biquad::process(float in) {
    double out = in * a0 + z1;
    z1 = in * a1 + z2 - b1 * out;
    z2 = in * a2 - b2 * out;
    return float(out);
}

#endif // Biquad_h
