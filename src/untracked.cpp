#include "../include/untracked.h"
#include "../include/platform.h"
#include <iostream>

static void print_usage() {
	std::cout << "usage: plsync untracked <platform>\n\n"
			  << untracked::description << "\n\n"
			  << "Options:\n"
			  << "  platform  Name of platform to view playlists from. Can either be 'yt' or a prefix of 'youtube' and 'spotify'\n";
}

static Platform parse_args(int argc, char *argv[]) {
	if (argc != 1 || strcmp(argv[0], "-h")==0 || strcmp(argv[0], "--help")==0) {
		print_usage();
		exit(1);
	}

	const char *platform = argv[0];
	const char *youtube = "youtube";
	const char *spotify = "spotify";

	if (strcmp(platform, "yt") == 0) {
		return Platform::YOUTUBE;
	} 
	if (std::equal(platform, platform+strlen(platform), youtube)) {
		return Platform::YOUTUBE;
	}
	if (std::equal(platform, platform+strlen(platform), spotify)) {
		return Platform::SPOTIFY;
	}
	print_usage();
	exit(1);
}

int run_untracked(int argc, char *argv[]) {
	Platform platform = parse_args(argc, argv);
	return 0;
}
