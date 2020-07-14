
/*
	FM. BISON -- MIDI constants & note freq. LUT.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-MIDI.h"

namespace SFM
{
	float g_MIDIToFreqLUT[kMIDINumKeys];

	void CalculateMIDIToFrequencyLUT()
	{
		const float base = kBaseHz;
		for (unsigned iKey = 0; iKey < kMIDINumKeys; ++iKey)
		{
			const float frequency = base * powf(2.f, (iKey-69.f)/12.f);
			g_MIDIToFreqLUT[iKey] = fabsf(frequency);
		}
	}
}
