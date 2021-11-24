	
/*
	FM. BISON hybrid FM synthesis -- Linkwitz-Riley filter (crossover).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

    What is this 'LR' filter? https://en.wikipedia.org/wiki/Linkwitz%E2%80%93Riley_filter    

    In short it is a filter that's mosten often used for band separation; these filters are widely used as crossovers
    in for example speakers or mixers.

    A few key properties:
    - The sum of both the LPF and HPF has a flat (unit) magnitude frequency response curve, ergo, it retains equal power.
    - The Butterworth filter is ideally suitable to construct this filter as it has zero ripple in the passband and beyond.
    - As with any 4th-order filter this one has a -24dB per octave (logarithmic) slope.

    FIXME: WIP!
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
    class LinkwitzRiley
    {
    public:
        LinkwitzRiley(unsigned sampleRate) :
            m_sampleRate(sampleRate)
        {
        }

        ~LinkwitzRiley() {}

        void Reset();
        void SetCutoff(float Fc);

    private:
        const unsigned m_sampleRate;
    };
}
