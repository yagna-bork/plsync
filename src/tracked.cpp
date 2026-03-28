#include "../include/tracked.h"
#include "../include/config.h"
#include "../include/playlist_items_cache.h"
#include "../include/platform.h"
#include "../include/playlist_cache.h"
#include "../include/util.h"
#include "../include/new_api.h"
#include "../include/token_store.h"
#include <stddef.h>
#include <filesystem>
#include <iostream>
#include <forward_list>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

static void print_usage() {
	std::cout << "usage: plsync tracked\n\n" << tracked_description << '\n';
}

static bool parse_args(int argc, char* argv[]) {
	if (argc > 0) {
		print_usage();
		return false;
	}
	return true;
}

int save_cache_quit(std::forward_list<PlaylistItems>& cache) {
	save_playlist_items_cache(cache);
	std::cerr << "Something went wrong. Please try again.\n";
	return 1;
}

int run_tracked(int argc, char* argv[]) {
	if (!parse_args(argc, argv)) {
		return 0;
	}
	std::vector<std::size_t> plat_to_sid_len(Platform::INVALID, 0);
	size_t longest_sid = 0;
	for (int i = 0; i != Platform::INVALID; i++) {
		Platform plat = static_cast<Platform>(i);
		PlaylistCache::Head head;
		if (PlaylistCache::load_head(plat, head)) {
			plat_to_sid_len[plat] = head.sid_len;
			longest_sid = std::max(longest_sid, head.sid_len);
		}
	}

	auto curl = get_curl();
	std::forward_list<PlaylistItems> cache = load_playlist_items_cache();
	try {
		update_playlist_items_cache(cache, curl);
	} catch (const TokenStorageAccessError& e) {
		return save_cache_quit(cache);
	} catch (const API::RequestError& e) {
		return save_cache_quit(cache);
	}

	size_t id_pad = longest_sid*2 - 1;
	std::ostringstream heading_ss;
	heading_ss << "Platform Id" << std::string(id_pad, ' ') << "Title";
	std::string heading = heading_ss.str();
	std::cout << heading << '\n' << std::string(heading.size(), '-') << '\n';

	bool skip_newline = true;
	for (const auto& pl_items: cache) {
		if (skip_newline) {
			skip_newline = false;
		} else {
			std::cout << '\n';
		}
		for (const auto& [plat, pl]: pl_items.tracked) {
			size_t plat_pad = 9 - platform_title(plat).size();
			id_pad = (longest_sid - pl.short_id.size())*2 + 1;
			std::cout << platform_title(plat) << std::string(plat_pad, ' ') 
					  << bin_to_hex(pl.short_id) << std::string(id_pad, ' ')
					  << pl.title << '\n';
		}
		std::cout << pl_items.song_hashes.size() << " song(s)\n";
	}
	save_playlist_items_cache(cache);
	return 0;
}
