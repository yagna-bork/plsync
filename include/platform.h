#ifndef GUARD_PLATFORM_H
#define GUARD_PLATFORM_H
#include <vector>
#include <string>

enum Platform { YOUTUBE = 0, SPOTIFY = 1 };

std::string title(Platform p);

std::string title_lower(Platform p);

std::string abbrev(Platform p);
#endif
