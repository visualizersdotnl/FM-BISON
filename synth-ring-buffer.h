
/*
	FM. BISON hybrid FM synthesis -- Audio streaming (lockless) ring buffer.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!

	FIXME: 
		- Templatize?
*/

#pragma once

#include "synth-global.h"

namespace SFM
{
	class RingBuffer
	{
	public:
		RingBuffer(size_t size) :
			m_size(size)
,			m_buffer((float *) mallocAligned(size * sizeof(float), 16))
,			m_readIdx(0)
,			m_writeIdx(0)
		{
			memset(m_buffer, 0, size * sizeof(float));
		}

		~RingBuffer()
		{
			freeAligned(m_buffer);
		}

		SFM_INLINE void Write(float value)
		{
			m_buffer[m_writeIdx % m_size] = value;
			++m_writeIdx;
		}

		SFM_INLINE float Read()
		{
			// Underrun?
			SFM_ASSERT(m_readIdx < m_writeIdx);

			const float value = m_buffer[m_readIdx % m_size];
			++m_readIdx;

			return value;
		}

		void Flush(float *pDest, unsigned numElements)
		{
			SFM_ASSERT(numElements <= m_size);
			
			// Underrun?
			SFM_ASSERT(m_readIdx < m_writeIdx);

			const unsigned range = m_readIdx+numElements;
			const unsigned head = range % m_size;
			const unsigned tail = numElements-head;

			memcpy(pDest,      m_buffer+m_readIdx, head * sizeof(float));
			memcpy(pDest+head, m_buffer,           tail * sizeof(float));

			m_readIdx = range;
		}

		SFM_INLINE unsigned GetAvailable() const
		{
			return m_writeIdx-m_readIdx;
		}

		SFM_INLINE bool IsFull() const
		{
			return m_writeIdx == m_readIdx+m_size;
		}
		
	private:
		const size_t m_size;
		float *m_buffer;

		unsigned m_readIdx;
		unsigned m_writeIdx;
	};
}

	