#include "../include/platform.h"

std::string platform_title(Platform p) {
    static const std::vector<std::string> titles = {"YouTube", "Spotify"
#ifndef NDEBUG
                                                    ,
                                                    "Test"
#endif
    };
    return titles[p];
};

std::string platform_title_lower(Platform p) {
    std::string t = platform_title(p);
    std::transform(t.begin(), t.end(), t.begin(), tolower);
    return t;
};

std::string platform_abbrev(Platform p) {
    static const std::vector<std::string> abbrevs = {"yt", "sp"
#ifndef NDEBUG
                                                     ,
                                                     "tt"
#endif
    };
    return abbrevs[p];
}

Platform parse_platform(const std::string& s) {
    if (s == "yt") {
        return Platform::YOUTUBE;
    }
    for (int i = 0; i != Platform::INVALID; i++) {
        Platform p = static_cast<Platform>(i);
        std::string p_str = platform_title_lower(p);
        if (std::equal(s.begin(), s.end(), p_str.begin())) {
            return p;
        }
    }
    return Platform::INVALID;
}
