#include "../include/untrack.h"
#include "../include/platform.h"
#include "../include/sid_to_id_map.h"
#include "../include/playlist_cache.h"
#include "../include/playlist_items_cache.h"
#include <utility>
#include <string>
#include <stdexcept>
#include <iostream>

static void print_usage() {
	std::cout << "usage: plsync untrack <platform> <playlist-id>\n\n"
			  << untrack_description << "\n\n"
			  << "Options:\n"
			  << "  platform     The platform that the playlist belongs to. Can either be 'yt' or a prefix of 'youtube' and 'spotify'\n"
			  << "  playlist-id  Value of the id field shown in the output of <tracked> for the playlist to untrack\n";
}

static void parse_args(int argc, char* argv[], Platform& plat, PlaylistCache::Node& node) {
	if (argc != 2) {
		throw std::invalid_argument("");
	}
	if (parse_platform(argv[0]) == Platform::INVALID) {
		throw std::invalid_argument("");
	}
	if (strlen(argv[1]) == 0 || strlen(argv[1]) % 2 != 0) {
		throw std::invalid_argument("");
	}
	plat = parse_platform(argv[0]);
	std::string playlist_id = sid_to_id_lookup(hex_to_bin(argv[1]), plat);
	if (playlist_id.empty()) {
		throw std::invalid_argument("");
	}
	node = PlaylistCache::load_node(playlist_id, plat);
	if (node.items_id.empty()) {
		throw std::invalid_argument("");
	}
}

int untrack(int argc, char* argv[]) {
	Platform plat;
	PlaylistCache::Node node;
	parse_args(argc, argv, plat, node);
    auto pl_items = PlaylistItemsCache::load(node.items_id);
	if (pl_items.tracked_playlists.size() == 2) {
		PlaylistItemsCache::remove(pl_items.id);
		return 0;
	}

	for (auto it = pl_items.tracked_playlists.begin(); it != pl_items.tracked_playlists.end(); it++) {
		if (it->id == node.playlist.id) {
			pl_items.tracked_playlists.erase(it);
			break;
		}
	}
	PlaylistItemsCache::save(pl_items);
	node.items_id.clear();
	PlaylistCache::save_node(node, plat);
	return 0;
}

int run_untrack(int argc, char* argv[]) {
	try {
		return untrack(argc, argv);
	} catch (const std::invalid_argument& e) {
		print_usage();
		return 1;
	} catch (const SidToIdMapUninitialisedError& e) {
		print_usage();
		return 1;
	}
}
