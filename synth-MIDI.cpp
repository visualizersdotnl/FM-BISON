
/*
	FM. BISON -- MIDI constants & note freq. LUT.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-global.h"
#include "synth-MIDI.h"

namespace SFM
{
	double g_MIDIToFreqLUT[kMIDINumKeys];

	void CalculateMIDIToFrequencyLUT()
	{
		const double base = kBaseHz;
		for (unsigned iKey = 0; iKey < kMIDINumKeys; ++iKey)
		{
			const double frequency = base * pow(2.0, (iKey-69.0)/12.0);
			g_MIDIToFreqLUT[iKey] = fabs(frequency);
		}
	}
}
