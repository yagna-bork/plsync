#include "../include/tracked.h"
#include "../include/config.h"
#include "../include/playlist_items_cache.h"
#include "../include/platform.h"
#include "../include/playlist_cache.h"
#include "../include/util.h"
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

	std::forward_list<PlaylistItems> cache = load_playlist_items_cache();
	size_t id_pad = longest_sid*2 - 2;
	std::ostringstream heading_ss;
	heading_ss << "Platform Id" << std::string(id_pad, ' ');
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
			std::string id_hash;
			sha256(pl.id, id_hash);
			std::string sid(id_hash.begin(), id_hash.begin() + plat_to_sid_len[plat]);
			size_t plat_pad = 9 - platform_title(plat).size();
			std::cout << platform_title(plat) << std::string(plat_pad, ' ') << bin_to_hex(sid) << '\n';
		}
		std::cout << pl_items.song_hashes.size() << " song(s)\n";
	}
	return 0;
}
