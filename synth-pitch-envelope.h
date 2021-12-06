
/*
	FM. BISON hybrid FM synthesis -- Simple exponential envelope with 4 control points intended to use as a pitch envelope.
	(C) njdewit technologies (visualizers.nl) & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	This is a pretty standard envelope that remotely mimicks the Yamaha DX7's, just so I could get a little decent pitch
	envelope support. What it will do is interpolate until P4 has been reached, and if P4 loops (i.e. L4 isn't zero), it
	will return to P1 and cycle.

	If the envelope is stopped at any point it will interpolate towards P4.

	The range returned is [-1..1], it should be adjusted to pitch range.

	FIXME:
 		- Let envelope hold at P3 (sustain), on release interpolate to P4
		- Refactor code (duplication, ham-fisted approach, et cetera)
		- Curves? Ask Paul!
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	class PitchEnvelope
	{
	private:
		// In samples
		SFM_INLINE unsigned CalcRate(float rate)
		{
			return unsigned(m_sampleRate*m_parameters.globalMul*rate);
		}

	public:
		struct Parameters
		{
			// FIXME: use arrays
			float P1, P2, P3, P4; // [-1..1]
			float R1, R2, R3, L4;
			float globalMul; // In seconds
		};

		void Start(const Parameters &parameters, unsigned sampleRate)
		{
			m_parameters = parameters;
			m_sampleRate = sampleRate;

			m_curPoint = 0;
			m_iSample = 0;
			m_curLevel = m_parameters.P1;

			m_releaseLevel = parameters.P4;
			
			m_rates[0] = CalcRate(m_parameters.R1);
			m_rates[1] = CalcRate(m_parameters.R2);
			m_rates[2] = CalcRate(m_parameters.R3);
			m_rates[3] = CalcRate(m_parameters.L4);
		}

		SFM_INLINE float Sample(bool sustained)
		{
			switch (m_curPoint)
			{
			case 0:
				{
					const unsigned numSamples = m_rates[0];
					if (m_iSample < numSamples)
					{
						m_curLevel = Interpolate(m_parameters.P1, m_parameters.P2, numSamples);
						break;
					}
					else
					{
						m_iSample = 0;
						++m_curPoint;
					}
				}

			case 1:
				{
					const unsigned numSamples = m_rates[1];
					if (m_iSample < numSamples)
					{
						m_curLevel = Interpolate(m_parameters.P2, m_parameters.P3, numSamples);
						break;
					}
					else
					{
						m_iSample = 0;
						++m_curPoint;
					}
				}

			case 2:
				{
					const unsigned numSamples = m_rates[2];
					if (m_iSample < numSamples)
					{
						m_curLevel = Interpolate(m_parameters.P3, m_parameters.P4, numSamples);
						break;
					}
					else
					{
						m_iSample = 0;
						++m_curPoint;
					}
				}

			case 3:
				{
					if (0.f != m_parameters.L4)
					{
						// Loop
						const unsigned numSamples = m_rates[3];
						m_curLevel = Interpolate(m_releaseLevel, m_parameters.P1, numSamples);
					
						if (m_iSample == numSamples)
						{
							m_iSample = 0;
							m_curPoint = 0; // Wrap around
						}
					}
					else
						m_curLevel = m_releaseLevel;

					break;
				}

			default:
				SFM_ASSERT(false);
			}

			if (false == sustained && m_curPoint != 2)
				++m_iSample;

			return m_curLevel;
		}

		void Stop()
		{
			// Start last section (P4->P1)
			m_iSample = 0;
			m_curPoint = 3;
			
			// At current level
			m_releaseLevel = m_curLevel;
		}

		void Reset(unsigned sampleRate)
		{
			Parameters parameters;
			memset(&parameters, 0, sizeof(Parameters));

			Start(parameters, sampleRate);
		}

	private:
		/* const */ Parameters m_parameters;
		/* const */ unsigned m_sampleRate;

		unsigned m_curPoint;
		unsigned m_iSample;
		float    m_curLevel;
		
		float m_releaseLevel;

		unsigned m_rates[4];

		SFM_INLINE float Interpolate(float from, float to, unsigned numSamples)
		{
			const float delta = to-from;

			float step = 1.f/numSamples;
			step *= m_iSample;
			
			return from + step*delta;
		}
	};
}
