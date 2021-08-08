
/*
	FM. BISON hybrid FM synthesis -- Mini EQ (3-band).
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-mini-EQ.h"

namespace SFM
{
	void MiniEQ::SetTargetdBs(float bassdB, float trebledB, float middB /* = 0.f */)
	{
		SFM_ASSERT(bassdB   >= kMiniEQMindB && bassdB   <= kMiniEQMaxdB); 
		SFM_ASSERT(trebledB >= kMiniEQMindB && trebledB <= kMiniEQMaxdB); 
		SFM_ASSERT(middB    >= kMiniEQMindB && middB    <= kMiniEQMaxdB);

		// FIXME: why?
		bassdB   += kEpsilon;
		trebledB += kEpsilon;
		middB    += kEpsilon;

		m_bassdB.SetTarget(bassdB);
		m_trebledB.SetTarget(trebledB);
		m_middB.SetTarget(middB);
	}
}
