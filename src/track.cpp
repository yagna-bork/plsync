#include "../include/track.h"
#include "../include/platform.h"
#include "../include/sid_to_id_map.h"
#include "../include/util.h"
#include "../include/playlist_cache.h"
#include "../include/new_api.h"
#include "../include/token_store.h"
#include "../include/playlist_items.h"
#include <iostream>
#include <utility>
#include <unordered_set>
#include <stdexcept>
#include <curl/curl.h>

using namespace PlaylistCache;

static void print_usage() {
	std::cout << "usage: plsync track <platform> <playlist-id>\n\n"
			  << track_description << "\n\n"
			  << "Options:\n"
			  << "  platform     Name of platform to view playlists from. Can either be 'yt' or a prefix of 'youtube' and 'spotify'\n"
			  << "  playlist-id  Value of the id field shown in the output of <untracked> for the playlist to track\n";
}

static std::unordered_map<Platform, std::string> parse_args(int argc, char *argv[]) {
	if (argc < 3) {
		throw std::invalid_argument("");
	}

	std::unordered_map<Platform, std::string> plat_to_sid;
	Platform prev_plat = Platform::INVALID;
	for (int i = 0; i != argc; i++) {
		auto plat = parse_platform(argv[i]);
		if (plat != Platform::INVALID) {
			if (plat_to_sid.count(plat)) {
				throw std::invalid_argument("");
			}
			plat_to_sid[plat] = "";
			prev_plat = plat;
			continue;
		}
		if (prev_plat == Platform::INVALID || plat_to_sid.count(plat) || std::strlen(argv[i]) % 2 != 0) {
			throw std::invalid_argument("");
		}
		plat_to_sid[prev_plat] = argv[i];
	}

	auto sid_empty = [](const std::pair<Platform, std::string>& pair) { return pair.second.empty(); };
	if (plat_to_sid.size() < 2 || std::all_of(plat_to_sid.begin(), plat_to_sid.end(), sid_empty)) {
		throw std::invalid_argument("");
	}
	return plat_to_sid;
}

static void track(int argc, char *argv[]) {
	std::unordered_map<Platform, std::string> plat_to_sid = parse_args(argc, argv);
	std::shared_ptr<CURL> curl = get_curl();
	std::unordered_map<Platform, Node> plat_to_node;
	std::string items_id;
	std::string playlist_title;
	for (const auto& pair: plat_to_sid) {
		auto plat = pair.first;
		const auto& sid = pair.second;
		if (sid.empty()) {
			continue;
		}

		// check sid is valid
		std::string id = sid_to_id_lookup(hex_to_bin(sid), plat);
		if (id.empty()) {
			throw std::invalid_argument("");
		}
		Node node = load_node(id, plat);
	
		// check playlist wasn't deleted
		std::string access_tkn = get_or_refresh_access_tkn(plat, curl);
		Playlist modified_playlist;
		bool modified = API::get_playlist(plat, curl.get(), access_tkn, id, node.playlist.etag, modified_playlist);
		if (modified) {
			if (modified_playlist.id.empty()) {
				// playlist was deleted
				delete_node(node, plat);
				throw std::invalid_argument("");
			} else {
				node.playlist = modified_playlist;
				save_node(node, plat);
			}
		}
		
		// check at most one playlist is already tracked
		// TODO test
		if (!node.items_id.empty()) {
			if (!items_id.empty()) {
				throw std::invalid_argument("");
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
	for (const auto& pair: plat_to_sid) {
		if (!pair.second.empty()) {
			continue;
		}
		Platform plat = pair.first;
		std::string access_tkn = get_or_refresh_access_tkn(plat, curl);
		Playlist playlist = API::create_playlist(plat, curl.get(), access_tkn, playlist_title);
		Node node(playlist);
		create_node(node, plat);
		plat_to_node[plat] = std::move(node);
	}
	
	// and finally track the provided playlists
	PlaylistItems pl_items;
	if (items_id.empty()) {
		pl_items.id = bin_to_hex(rndstr(16));
	} else {
		// TODO test
		pl_items = load_playlist_items(items_id);
	}

	for (auto& pair: plat_to_node) {
		auto& plat = pair.first;
		auto& node = pair.second;
		if (!node.items_id.empty()) {
			continue;
		}
		pl_items.tracked_playlists.emplace_back(plat, node.playlist.id, /*items_etag=*/"");
		node.items_id = pl_items.id;
		save_node(node, plat);
	}
	save_playlist_items(pl_items);
}

int run_track(int argc, char *argv[]) {
	try {
		track(argc, argv);
		return 0;
	} catch (const std::invalid_argument& e) {
		print_usage();
	} catch (const SidToIdMapUninitialisedError& e) {
		print_usage();
	} catch (const std::exception& e) {
		std::cerr << "Something went wrong please try again\n";
	}
	return 1;
}
