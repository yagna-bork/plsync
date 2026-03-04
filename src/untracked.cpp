#include "../include/untracked.h"
#include "../include/platform.h"
#include "../include/util.h"
#include "../include/api.h"
#include "../include/token_store.h"
#include "../include/models.h"
#include "../include/playlist_cache.h"
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

	std::vector<Playlist> playlists;
	// TODO is this etag even relevant?
	std::string etag;
	try {
		api->get_playlists(playlists, etag);
	} catch (const BaseAPI::RequestError &e) {
		std::cerr << "Something went wrong. Try again.\n";
		return 1;
	}
	
	CacheHead* head = load_cache(platform);
	update_cache(head, playlists);
	
	// find longest title for even formatting
	CacheNode *node = head->next;
	std::size_t longest_title = 0;
	while (node) {
		longest_title = std::max(longest_title, node->playlist.title.size());
		node = node->next;
	}

	// TODO needs compression, probs iterator if it's quick enough
	node = head->next;
	while (node) {
		const auto& pl = node->playlist;
		std::size_t title_pad = longest_title + 1 - pl.title.size();
		std::string privacy_type = pl.is_private ? "private" : "public";
		std::size_t privacy_pad = pl.is_private ? 1 : 2;
		std::cout << pl.title << std::string(title_pad, ' ') << " " 
				  << privacy_type << std::string(privacy_pad, ' ') << " " 
				  << pl.items << '\n';
		node = node->next;
	}
	free_cache(head, platform);
	return 0;
}
