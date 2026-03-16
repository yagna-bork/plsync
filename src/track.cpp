#include "../include/track.h"
#include "../include/platform.h"
#include "../include/sid_to_id_map.h"
#include "../include/util.h"
#include "../include/playlist_cache.h"
#include "../include/new_api.h"
#include "../include/token_store.h"
#include <iostream>
#include <utility>
#include <unordered_set>
#include <curl/curl.h>

using PlatSidPair = std::pair<Platform, std::string>;
using PlatSidPairs = std::vector<PlatSidPair>;
using namespace PlaylistCache;

static void print_usage_exit() {
	std::cout << "usage: plsync track <platform> <playlist-id>\n\n"
			  << track::description << "\n\n"
			  << "Options:\n"
			  << "  platform     Name of platform to view playlists from. Can either be 'yt' or a prefix of 'youtube' and 'spotify'\n"
			  << "  playlist-id  Value of the id field shown in the output of <untracked> for the playlist to track\n";
	exit(1);
}

PlatSidPairs parse_args(int argc, char *argv[]) {
	if (argc == 1) {
		print_usage_exit();
	}

	std::unordered_set<Platform> plats;
	PlatSidPairs plat_sid_pairs;
	for (int i = 0; i != argc; i++) {
		auto plat = parse_platform(argv[i]);
		if (plat != Platform::INVALID) {
			if (plats.count(plat)) print_usage_exit();
			plat_sid_pairs.emplace_back(plat, "");
			plats.insert(plat);
			continue;
		}
		// make sure previous arg was a platform
		if (plat_sid_pairs.size() == 0 || !plat_sid_pairs.back().second.empty()) {
			print_usage_exit();
		}
		if (std::strlen(argv[i]) % 2 != 0) {
			print_usage_exit();
		}
		plat_sid_pairs.back().second = argv[i];
	}
	
	auto sid_empty = [](const PlatSidPair& pair) { return pair.second.empty(); };
	if (std::all_of(plat_sid_pairs.begin(), plat_sid_pairs.end(), sid_empty)) {
		print_usage_exit();
	}
	return plat_sid_pairs;
}

int run_track(int argc, char *argv[]) {
	// TODO make map
	PlatSidPairs plat_sid_pairs = parse_args(argc, argv);
	if (plat_sid_pairs.size() < 2) {
		print_usage_exit();
	}
	std::shared_ptr<CURL> curl = get_curl();

	std::unordered_map<Platform, Node> plat_to_node;
	std::string items_id;
	std::string playlist_title;
	for (const auto& pair: plat_sid_pairs) {
		Platform plat = pair.first;
		const auto& sid = pair.second;
		if (sid.empty()) {
			continue;
		}

		// check sid is valid
		std::string id = sid_to_id_lookup(hex_to_bin(sid), plat);
		if (id.empty()) {
			print_usage_exit();
		}
		Node node = load_node(id, plat);
	
		// check playlist wasn't deleted
		std::string access_tkn;
		if (!get_or_fetch_access_tkn(plat, curl, access_tkn)) {
			std::cerr << "Couldn't get " << platform_title(plat) << " access token. Please try again\n";
			return 1;
		}

		bool modified;
		Playlist modified_playlist;
		try {
			modified = API::get_playlist(plat, curl.get(), access_tkn, id, node.playlist.etag, modified_playlist);
		} catch (const API::RequestError& e) {
			std::cerr << "Something went wrong please try again\n";
			exit(1);
		}

		if (modified) {
			if (modified_playlist.id.empty()) {
				// playlist was deleted
				delete_node(node, plat);
				print_usage_exit();
			} else {
				node.playlist = modified_playlist;
				save_node(node, plat);
			}
		}
		
		// check at most one playlist is already tracked
		if (!node.items_id.empty()) {
			if (!items_id.empty()) {
				print_usage_exit();
			} else {
				items_id = node.items_id;
			}
		}
		if (playlist_title.empty()) {
			playlist_title = node.playlist.title;
		}
		plat_to_node[plat] = std::move(node);
	}
	
	// create playlists for platforms where it wasn't provided
	for (const auto& pair: plat_sid_pairs) {
		if (!pair.second.empty()) {
			continue;
		}

		Platform plat = pair.first;
		std::string access_tkn;
		if (!get_or_fetch_access_tkn(plat, curl, access_tkn)) {
			std::cerr << "Couldn't get " << platform_title(plat) << " access token. Please try again\n";
			return 1;
		}

		Playlist playlist;
		try {
			playlist = API::create_playlist(plat, curl.get(), access_tkn, playlist_title);
		} catch (const API::RequestError& e) {
			std::cerr << "Something went wrong. Please try again.\n";
			return 1;
		}
		Node node(playlist);
		create_node(node, plat);
		plat_to_node[plat] = std::move(node);
	}
	return 0;
}
