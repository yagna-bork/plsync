#include "../include/platform.h"

static const std::vector<std::string> titles = {"YouTube", "Spotify"};
static const std::vector<std::string> abbrevs = {"yt", "sp"};

std::string title(Platform p) { return titles[p]; };

std::string title_lower(Platform p) {
	std::string t = titles[p];
	std::transform(t.begin(), t.end(), t.begin(), tolower);
	return t;
};

std::string abbrev(Platform p) {
	return abbrevs[p];
}
