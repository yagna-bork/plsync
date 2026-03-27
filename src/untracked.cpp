#include "../include/untracked.h"
#include "../include/platform.h"
#include "../include/util.h"
#include "../include/api.h"
#include "../include/token_store.h"
#include "../include/models.h"
#include "../include/playlist_cache.h"
#include "../include/sid_to_id_map.h"
#include <stddef.h>
#include <assert.h>
#include <iostream>
#include <sstream>

namespace Cache = PlaylistCache;

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
	auto platform = parse_platform(argv[0]);
	if (platform == Platform::INVALID) {
		print_usage();
		exit(1);
	}
	return platform;
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

	Cache::Handle cache(platform);
	bool modified;
	std::vector<Playlist> modified_playlists;
	std::string modified_etag = cache.head->etag;
	try {
		modified = api->get_playlists(modified_playlists, modified_etag);
	} catch (const BaseAPI::RequestError &e) {
		std::cerr << "Something went wrong. Try again.\n";
		return 1;
	}

	int sid_len;
	if (modified) {
		Cache::update(cache.head, cache.plat, modified_playlists, modified_etag);
		sid_len = Cache::calculate_short_id_len(cache.head);
		Cache::fill_short_ids(cache.head, sid_len);
		update_sid_to_id_map(cache.head, cache.plat);
	} else {
		sid_len = cache.head->sid_len;
		Cache::fill_short_ids(cache.head, sid_len);
	}

	size_t longest_title = 0;
	for (const Playlist& pl: cache) {
		longest_title = std::max(longest_title, utf8_len(pl.title));
	}

	int id_pad = std::max(1, sid_len*2+1 - 2);
	int title_pad = std::max(size_t(1), longest_title - 4);
	std::stringstream heading_ss;
	heading_ss << "Id" << std::string(id_pad, ' ') 
		       << "Title" << std::string(title_pad, ' ') 
		       << "Privacy " << "Items";
	std::string heading = heading_ss.str();
	std::cout << heading << '\n';
	std::cout << std::string(heading.size(), '-') << '\n';

	// TODO sort
	// TODO use setw but also use manual padding with title
	for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
		if (!it.ptr.node->items_id.empty()) {
			continue;
		}
		id_pad = std::max(1, 3 - sid_len*2); 
		int title_pad = std::max(longest_title, size_t(5)) + 1 - utf8_len(it->title);
		std::string privacy_type = it->is_private ? "private" : "public";
		int privacy_pad = it->is_private ? 1 : 2;
		std::cout << bin_to_hex(it->short_id) << std::string(id_pad, ' ')
				  << it->title << std::string(title_pad, ' ')
				  << privacy_type << std::string(privacy_pad, ' ')
				  << it->items << '\n';
	}
	return 0;
}
