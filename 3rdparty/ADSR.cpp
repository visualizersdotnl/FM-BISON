
// Modified to fit FM. BISON (see ADSR.h)

//
//  ADSR.cpp
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
//  1.01  2016-01-02  njr   added calcCoef to SetTargetRatio functions that were in the ADSR widget but missing in this code
//  1.02  2017-01-04  njr   in calcCoef, checked for rate 0, to support non-IEEE compliant compilers
//

#include "ADSR.h"

#include <cmath>
#include <math.h>

namespace SFM 
{
	ADSR::ADSR(void) {
		reset();
		setAttackRate(0.0);
		setDecayRate(0.0);
		setReleaseRate(0.0);
		setSustainLevel(1.0);
		setTargetRatioA(0.3);
		setTargetRatioD(0.0001);
		setTargetRatioR(0.0001);
	}

	ADSR::~ADSR(void) {
	}

	void ADSR::setAttackRate(double rate) {
		attackRate = rate;
		attackCoef = calcCoef(rate, targetRatioA);
		attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
	}

	void ADSR::setDecayRate(double rate) {
		decayRate = rate;
		decayCoef = calcCoef(rate, targetRatioD);
		decayBase = (sustainLevel-targetRatioD) * (1.0 - decayCoef);
	}

	void ADSR::setReleaseRate(double rate) {
		releaseRate = rate;
		releaseCoef = calcCoef(rate, targetRatioR);
		releaseBase = -targetRatioR * (1.0 - releaseCoef);
	}

	double ADSR::calcCoef(double rate, double targetRatio) 
	{
		return (rate <= 0.0) ? 0.0 : exp(-log((1.0 + targetRatio) / targetRatio) / rate);
	}

	void ADSR::setSustainLevel(double level) {
		sustainLevel = level;
		decayBase = (sustainLevel-targetRatioD) * (1.0 - decayCoef);
	}

	void ADSR::setTargetRatioA(double targetRatio) {
		if (targetRatio < 0.000000001)
			targetRatio = 0.000000001;  // -180 dB
		targetRatioA = targetRatio;
		attackCoef = calcCoef(attackRate, targetRatioA);
		attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
	}

	void ADSR::setTargetRatioD(double targetRatio) {
		if (targetRatio < 0.000000001)
			targetRatio = 0.000000001;  // -180 dB
		targetRatioD = targetRatio;
		decayCoef = calcCoef(decayRate, targetRatioD);
		decayBase = (sustainLevel-targetRatioD) * (1.0 - decayCoef);
	}

	void ADSR::setTargetRatioR(double targetRatio) {
		if (targetRatio < 0.000000001)
			targetRatio = 0.000000001;  // -180 dB
		targetRatioR = targetRatio;
		releaseCoef = calcCoef(releaseRate, targetRatioR);
		releaseBase = -targetRatioR * (1.0 - releaseCoef);
	}

} // SFM
