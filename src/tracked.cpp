#include "../include/tracked.h"
#include "../include/config.h"
#include "../include/playlist_items_cache.h"
#include "../include/platform.h"
#include "../include/playlist_cache.h"
#include "../include/util.h"
#include <stddef.h>
#include <filesystem>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>

using namespace PlaylistItemsCache;
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

	// TODO denormalising title was a bad idea
	// we want to force refresh playlist with api each time
	// to keep cache up to date and see if a playlist has 
	// been deleted
	std::vector<PlaylistItems> pl_items_list = load_all();
	size_t longest_title = 0;
	for (const auto& pl_items: pl_items_list) {
		for (const auto& pl: pl_items.tracked_playlists) {
			longest_title = std::max(longest_title, utf8_len(pl.title));
		}
	}

	size_t id_pad = longest_sid*2 - 2;
	size_t title_pad = std::max(size_t(5), longest_title) - 4;
	std::ostringstream heading_ss;
	heading_ss << "Platform Title" << std::string(title_pad, ' ') << "Id" << std::string(id_pad, ' ');
	std::string heading = heading_ss.str();
	std::cout << heading << '\n' << std::string(heading.size(), '-') << '\n';

	for (int i = 0; i != pl_items_list.size(); i++) {
		if (i > 0) {
			std::cout << '\n';
		}
		for (const auto& pl: pl_items_list[i].tracked_playlists) {
			std::string id_hash;
			sha256(pl.id, id_hash);
			std::string sid(id_hash.begin(), id_hash.begin() + plat_to_sid_len[pl.plat]);
			title_pad = std::max(size_t(5), longest_title) + 1 - utf8_len(pl.title);
			size_t plat_pad = 9 - platform_title(pl.plat).size();
			std::cout << platform_title(pl.plat) << std::string(plat_pad, ' ') 
					  << pl.title << std::string(title_pad, ' ') << bin_to_hex(sid) << '\n';
		}
		std::cout << pl_items_list[i].song_hashes.size() << " song(s)\n";
	}
	return 0;
}
