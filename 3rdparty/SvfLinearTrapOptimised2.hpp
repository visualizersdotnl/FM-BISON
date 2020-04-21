
//
// Modified to fit FM. BISON:
// - Fitted to compile without dependencies
// - Stereo support
// - Added a few setup functions
// - Stability assertion
// - Added a few smaller update functions for specific filter types
//
// FIXME:
// - try using fast(er) trig. functions (stress test filter stability before using permanently)
//

//  SvfLinearTrapOptimised2.hpp
//
//  Created by Fred Anton Corvest (FAC) on 26/11/2016.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.

#ifndef SvfLinearTrapOptimised2_hpp
#define SvfLinearTrapOptimised2_hpp

// #include "../synth-math.h"   
#include "../synth-helper.h" // FloatAssert()

/*!
 @class SvfLinearTrapOptimised2
 @brief A ready to use C++ port of Andy Simper's implementation of State Variable Filters described in the technical paper 
 provided at http://www.cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf Thank you Andy for the time you spent on making those technical papers.
 */
class SvfLinearTrapOptimised2 {
public:
	/*!
	 @class SvfLinearTrapOptimised2
	 @enum	FILTER_TYPE
	 @brief The different states of the filter.
	 */
	enum FLT_TYPE {LOW_PASS_FILTER, BAND_PASS_FILTER, HIGH_PASS_FILTER, NOTCH_FILTER, PEAK_FILTER, ALL_PASS_FILTER, BELL_FILTER, LOW_SHELF_FILTER, HIGH_SHELF_FILTER, NO_FLT_TYPE};
	
	SvfLinearTrapOptimised2() {
		_ic1eq_left = _ic2eq_left = _v1_left = _v2_left = _v3_left = 0.;
		_ic1eq_right = _ic2eq_right = _v1_right = _v2_right = _v3_right = 0.;
	}
	
	/*!
	 @class SvfLinearTrapOptimised2
	 @param gainDb
		Gain in dB to boost or cut the cutoff point of the Low shelf filter
	 */
	void setGain(double gainDb) {
		_coef.setGain(gainDb);
	}
	
	/*!
	 @class FacAbstractFilter
	 @brief Updates the coefficients of the filter for the given cutoff, q, type and sample rate
	 @param cutoff
		cutoff frequency in Hz that should be clamped into the range [16hz, NYQUIST].
	 @param Q
		Q factor that should be clamped into the range [0.025, 40.0]. Default value is 0.5
	 @param sampleRate
		Sample rate. Default value is 44100hz. Do not forget to call resetState before changing the sample rate
	 */
	SFM_INLINE void updateCoefficients(double cutoff, double q = 0.5, FLT_TYPE type = LOW_PASS_FILTER, double sampleRate = 44100) {
		_coef.update(cutoff, q, type, sampleRate);
	}

	// Quicker versions for PostPass::Apply() & Reverb::Apply()
	SFM_INLINE void updateAllpassCoeff(double cutoff, double q, double sampleRate) {
		_coef.updateAllpass(cutoff, q, sampleRate);
	}

	SFM_INLINE void updateLowpassCoeff(double cutoff, double q, double sampleRate) {
		_coef.updateLowpass(cutoff, q, sampleRate);
	}

	SFM_INLINE void updateHighpassCoeff(double cutoff, double q, double sampleRate) {
		_coef.updateHighpass(cutoff, q, sampleRate);
	}
	
	/*!
	 @class FacAbstractFilter
	 @brief Resets the state of the filter
			Do not forget to call resetState before changing the sample rate
	 */
	void resetState() {
		_ic1eq_left = _ic2eq_left = _ic1eq_right = _ic2eq_right = 0.;
	}

private:
	/*!
	 @class FacAbstractFilter
	 @brief Tick method (apply the filter on the provided sample & state (return single prec. floating point))
	 */
	SFM_INLINE float tickImpl(float v0, double &_v1, double &_v2, double &_v3, double &_ic1eq, double &_ic2eq) {
		_v3 = v0 - _ic2eq;
		_v1 = _coef._a1*_ic1eq + _coef._a2*_v3;
		_v2 = _ic2eq + _coef._a2*_ic1eq + _coef._a3*_v3;
		_ic1eq = 2.*_v1 - _ic1eq;
		_ic2eq = 2.*_v2 - _ic2eq;
		
		return float(_coef._m0*v0 + _coef._m1*_v1 + _coef._m2*_v2);
	}

public:    
	/*!
	 @class FacAbstractFilter
	 @brief Tick method (apply the filter on the provided stereo material (single prec. floating point))
	 */
	SFM_INLINE void tick(float &left, float &right) {
		const float v0_left  = tickImpl(left,  _v1_left,  _v2_left,  _v3_left,  _ic1eq_left,  _ic2eq_left);
		const float v0_right = tickImpl(right, _v1_right, _v2_right, _v3_right, _ic1eq_right, _ic2eq_right);
		left  = v0_left;
		right = v0_right;

		// Check if filter hasn't blown up; maybe SampleAssert() would be more correct here but I'm not sure
		// if that strict range applies or even has to apply here (FIXME)
		SFM::FloatAssert(left);
		SFM::FloatAssert(right);
	}
	
	// Do *not* mix with stereo tick() call without first calling resetState()
	SFM_INLINE void tickMono(float &sample) {
		sample = tickImpl(sample,  _v1_left,  _v2_left,  _v3_left,  _ic1eq_left,  _ic2eq_left);

		// Same as in tick()
		SFM::FloatAssert(sample);
	}

private:
	struct Coefficients {
		Coefficients() {
			_a1 = _a2 = _a3 = _m0 = _m1 = _m2 = 0;
			_A = _ASqrt = 1;
		}

		// Added for PostPass::Apply()
		SFM_INLINE void updateAllpass(double cutoff, double q, double sampleRate)
		{
			double g = tan((cutoff / sampleRate) * SFM::kPI);
//			const double g = SFM::fast_tanf((cutoff / sampleRate) * 0.5);
			const double k = computeK(q, false);

			computeA(g, k);
			_m0 = 1;
			_m1 = -2*k;
			_m2 = 0;
		}

		// Added for Reverb::Apply()
		SFM_INLINE void updateLowpass(double cutoff, double q, double sampleRate)
		{
			double g = tan((cutoff / sampleRate) * SFM::kPI);
//			const double g = SFM::fast_tanf((cutoff / sampleRate) * 0.5);
			const double k = computeK(q, false);

			computeA(g, k);
			_m0 = 0;
			_m1 = 0;
			_m2 = 1;
		}

		// Added for Reverb::Apply()
		SFM_INLINE void updateHighpass(double cutoff, double q, double sampleRate)
		{
			const double g = tan((cutoff / sampleRate) * SFM::kPI);
//			const double g = SFM::fast_tanf((cutoff / sampleRate) * 0.5);
			const double k = computeK(q, false);

			computeA(g, k);
			_m0 = 1;
			_m1 = -k;
			_m2 = -1;
		}
		
		SFM_INLINE void update(double cutoff, double q = 0.5, SvfLinearTrapOptimised2::FLT_TYPE type = LOW_PASS_FILTER, double sampleRate = 44100) {
			double g = tan((cutoff / sampleRate) * SFM::kPI);
//			double g = SFM::fast_tanf((cutoff / sampleRate) * 0.5);
			const double k = computeK(q, type == BELL_FILTER /* USE GAIN FOR BELL FILTER ONLY */);
			
			switch (type) {
				case LOW_PASS_FILTER:
					computeA(g, k);
					_m0 = 0;
					_m1 = 0;
					_m2 = 1;
					break;
				case BAND_PASS_FILTER:
					computeA(g, k);
					_m0 = 0;
					_m1 = 1;
					_m2 = 0;
					break;
				case HIGH_PASS_FILTER:
					computeA(g, k);
					_m0 = 1;
					_m1 = -k;
					_m2 = -1;
					break;
				case NOTCH_FILTER:
					computeA(g, k);
					_m0 = 1;
					_m1 = -k;
					_m2 = 0;
					break;
				case PEAK_FILTER:
					computeA(g, k);
					_m0 = 1;
					_m1 = -k;
					_m2 = -2;
					break;
				case ALL_PASS_FILTER:
					computeA(g, k);
					_m0 = 1;
					_m1 = -2*k;
					_m2 = 0;
					break;
				case BELL_FILTER:
					computeA(g, k);
					_m0 = 1;
					_m1 = k*(_A*_A - 1);
					_m2 = 0;
					break;
				case LOW_SHELF_FILTER:
					computeA(g /= _ASqrt, k);
					_m0 = 1;
					_m1 = k*(_A-1);
					_m2 = _A*_A - 1;
					break;
				case HIGH_SHELF_FILTER:
					computeA(g *= _ASqrt, k);
					_m0 = _A*_A;
					_m1 = k*(1-_A)*_A;
					_m2 = 1-_A*_A;
					break;
				case NO_FLT_TYPE:
					// nothing todo
					break;
				default:
					SFM_ASSERT(false);
			}
		}
		
		void setGain(double gainDb) {
			_A = pow(10.0, gainDb / 40.0);
			_ASqrt = sqrt(_A);
		}
		
		SFM_INLINE double computeK(double q, bool useGain=false) {
			return 1.f / (useGain ? (q*_A) : q);
		}
		
		SFM_INLINE void computeA(double g, double k) {
			_a1 = 1/(1 + g*(g + k));
			_a2 = g*_a1;
			_a3 = g*_a2;
		}

		double _g;
		
		double _a1, _a2, _a3;
		double _m0, _m1, _m2;
		
		double _A;
		double _ASqrt;
	} _coef;
	
	double _ic1eq_left;
	double _ic2eq_left;
	double _v1_left, _v2_left, _v3_left;

	double _ic1eq_right;
	double _ic2eq_right;
	double _v1_right, _v2_right, _v3_right;
};

#endif /* SvfLinearTrapOptimised2_hpp */
