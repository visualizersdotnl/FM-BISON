
/*
	FM. BISON hybrid FM synthesis -- Debug logging.
	(C) visualizers.nl & bipolaraudio.nl
	MIT license applies, please see https://en.wikipedia.org/wiki/MIT_License or LICENSE in the project root!
*/

#include "synth-global.h"
// #include "synth-log.h"

namespace SFM
{

#if SFM_NO_LOGGING // Set in synth-global.h

	void Log(const std::string &message) {}

#else

	void Log(const std::string &message)
	{
		// JUCE output
		DBG(message.c_str());
	}

#endif // SFM_NO_LOGGING

}
