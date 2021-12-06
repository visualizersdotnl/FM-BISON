
/*
	FM. BISON hybrid FM synthesis -- Aligned memory allocation.
	(C) njdewit technologies (visualizers.nl) & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#pragma once

#include <stdlib.h>

namespace SFM
{
	inline void* mallocAligned(size_t size, size_t align);
	inline void  freeAligned(void* address);

#ifdef _WIN32

	__forceinline void* mallocAligned(size_t size, size_t align) { return _aligned_malloc(size, align); }
	__forceinline void  freeAligned(void* address) { _aligned_free(address); }

#elif defined(__GNUC__)

	inline void* mallocAligned(size_t size, size_t align) 
	{ 
		void* address;
		posix_memalign(&address, align, size);
		return address;
	}

	inline void freeAligned(void* address) { free(address); }

#endif

}
