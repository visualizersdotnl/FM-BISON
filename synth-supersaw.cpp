
/*
	FM. BISON hybrid FM synthesis -- Supersaw utility class (handles JP-8000 approximate calculations).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	Information I've used: https://pdfs.semanticscholar.org/1852/250068e864215dd7f12755cf00636868a251.pdf
	All sampled JP-8000 data and their approximations are in this document, as well as a good number of implementation hints.
*/

#include "synth-supersaw.h"

namespace SFM
{
	static double DetuneToCurve(double detune)
	{
		// "Since the Roland JP-8000 is a hardware synthesizer, it uses MIDI protocol to transfer control data.
		//  MIDI values are from a scale of 0 to 127 (128 in total). The detune of the Super Saw is therefore
		//  divided into 128 steps. If the detune is sampled at every 8th interval, it will result in a total of 17
		//  (value 0 also included) data points."

		return 
			(10028.7312891634*pow(detune, 11.0)) - (50818.8652045924*pow(detune, 10.0)) + (111363.4808729368*pow(detune, 9.0)) -
			(138150.6761080548*pow(detune, 8.0)) + (106649.6679158292*pow(detune, 7.0)) - (53046.9642751875*pow(detune, 6.0))  + 
			(17019.9518580080*pow(detune, 5.0))  - (3425.0836591318*pow(detune, 4.0))   + (404.2703938388*pow(detune, 3.0))    - 
			(24.1878824391*pow(detune, 2.0))     + (0.6717417634*detune)                + 0.0030115596;		
	}

	void Supersaw::SetDetune(float detune /* [0..1] */)
	{
		SFM_ASSERT(detune >= 0.f && detune <= 1.f);

		// The function above seems expensive enough to skip if possible
		if (m_curDetune != detune)
		{
			m_curDetuneCurve = DetuneToCurve(detune);
			m_curDetune = detune;
		}
	}
}
