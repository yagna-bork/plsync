#include "../include/untracked.h"
#include "../include/platform.h"
#include "../include/util.h"
#include "../include/api.h"
#include "../include/token_store.h"
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
	std::string tkn;
	std::shared_ptr<CURL> curl = get_curl();
	if (!get_or_fetch_access_tkn(platform, curl, tkn)) {
		std::cerr << "Couldn't get access token. Please try again\n";
		return 1;
	}
	std::unique_ptr<BaseDataAPI> api = BaseDataAPI::get_api(platform, curl, tkn);

	std::vector<BaseDataAPI::Playlist> playlists;
	// TODO is this etag even relevant?
	std::string etag;
	try {
		api->get_playlists(playlists, etag);
	} catch (const BaseAPI::RequestError &e) {
		std::cerr << "Something went wrong. Try again.\n";
		return 1;
	}
	
	for (const auto &pl: playlists) {
		std::cout << pl.title << " " << (pl.is_private ? "private" : "public") << " " << pl.items << '\n';
	}

	
	return 0;
}
