#ifndef GUARD_INIT_H
#define GUARD_INIT_H
#include <string>

namespace init {
	const std::string description = "Allow OAuth permissions for Youtube and Spotify. Initially the only valid command.";
};

int run_init(bool init_youtube, bool init_spotify);
#endif
