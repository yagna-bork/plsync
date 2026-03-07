#include "../include/track.h"
#include "../include/platform.h"
#include "../include/sid_to_id_map.h"
#include "../include/util.h"
#include <iostream>

static void print_usage() {
	std::cout << "usage: plsync track <platform> <playlist-id>\n\n"
			  << track::description << "\n\n"
			  << "Options:\n"
			  << "  platform     Name of platform to view playlists from. Can either be 'yt' or a prefix of 'youtube' and 'spotify'\n"
			  << "  playlist-id  Value of the id field shown in the output of <untracked> for the playlist to track\n";
}

void parse_args(int argc, char *argv[], Platform& platform, std::string& playlist_sid) {
	if (argc != 2 || strcmp(argv[0], "-h")==0 || strcmp(argv[0], "--help")==0) {
		print_usage();
		exit(1);
	}

	const char *platform_arg = argv[0];
	const char *youtube = "youtube";
	const char *spotify = "spotify";

	if (strcmp(platform_arg, "yt") == 0) {
		platform = Platform::YOUTUBE;
	} else if (std::equal(platform_arg, platform_arg+strlen(platform_arg), youtube)) {
		platform = Platform::YOUTUBE;
	} else if (std::equal(platform_arg, platform_arg+strlen(platform_arg), spotify)) {
		platform = Platform::SPOTIFY;
	} else {
		print_usage();
		exit(1);
	}
	
	playlist_sid = argv[1];
}

int run_track(int argc, char *argv[]) {
	Platform platform;
	std::string playlist_sid_hex;
	parse_args(argc, argv, platform, playlist_sid_hex);

	std::string playlist_sid = hex_to_bin(playlist_sid_hex);
	SidToIdMap map = load_sid_to_id_map(platform);
	std::string playlist_id = map.count(playlist_sid) ? map[playlist_sid] : "";
	return 0;
}
