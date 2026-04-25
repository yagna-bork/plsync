#ifndef GUARD_PLATFORM_H
#define GUARD_PLATFORM_H
#include <string>
#include <vector>

enum Platform {
    YOUTUBE,
    SPOTIFY,
#ifndef NDEBUG
    TEST,
#endif
    INVALID // mark end of enum when iterating over it
};

/* Name of platform in capital case e.g. Youtube */
std::string platform_title(Platform p);

/* Name of platform in lower case e.g. youtube */
std::string platform_title_lower(Platform p);

/* Abbreviation of platform e.g. yt */
std::string platform_abbrev(Platform p);

Platform parse_platform(const std::string& s);
#endif
