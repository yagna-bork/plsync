#ifndef GUARD_PLATFORM_H
#define GUARD_PLATFORM_H
#include <vector>
#include <string>

enum Platform { 
	YOUTUBE = 0, 
	SPOTIFY = 1
#ifndef NDEBUG
	,TEST = 2
#endif
};

/* Name of platform in capital case e.g. Youtube */
std::string title(Platform p);

/* Name of platform in lower case e.g. youtube */
std::string title_lower(Platform p);

/* Abbreviation of platform e.g. yt */
std::string abbrev(Platform p);
#endif
