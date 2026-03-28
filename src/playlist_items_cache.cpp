#include "../include/playlist_items_cache.h"
#include "../include/cache.pb.h"
#include "../include/playlist_cache.h"
#include "../include/util.h"
#include "../include/config.h"
#include "../include/token_store.h"
#include "../include/new_api.h"
#include <cassert>
#include <ios>
#include <filesystem>
#include <fstream>
#include <forward_list>
#include <memory>
#include <vector>
#include <unordered_set>
#include <curl/curl.h>

namespace fs = std::filesystem;

inline fs::path dir() {
	return fs::path(get_setting("cache_dir")) / "playlist_items";
}

proto::Platform get_proto_platform(Platform plat) {
	switch (plat) {
		case Platform::YOUTUBE:
			return proto::Platform::YOUTUBE;
		case Platform::SPOTIFY:
			return proto::Platform::SPOTIFY;
		default:
			return proto::Platform::INVALID;
	}
}

Platform get_platform(proto::Platform plat) {
	switch (plat) {
		case proto::Platform::YOUTUBE:
			return Platform::YOUTUBE;
		case proto::Platform::SPOTIFY:
			return Platform::SPOTIFY;
		default:
			return Platform::INVALID;
	}
}

proto::PlaylistItems get_proto_items(const fs::path &path) {
	proto::PlaylistItems proto_items;
	{
		auto file = ensure_bin_file<std::ifstream>(path);
		proto_items.ParseFromIstream(&file);
	}
	return proto_items;
}

PlaylistItems load_from_path(const fs::path& path) {
	PlaylistItems items;
	auto proto_items = get_proto_items(path);
	for (const auto& p: proto_items.tracked()) {
		items.tracked.emplace_back(
			get_platform(p.plat()), 
			Playlist(std::string(p.playlist().id()), std::string(p.playlist().items_etag()))
		);
	}
	for (const auto& song: proto_items.song_hashes()) {
		items.song_hashes.emplace_back(song);
	}
	items.id = path.filename();
	return items;
}

PlaylistItems load_playlist_items(const std::string& id) {
	return load_from_path(dir() / id);
}

std::forward_list<PlaylistItems> load_playlist_items_cache() {
	std::forward_list<PlaylistItems> cache;
	for (const auto& path: fs::directory_iterator(dir())) {
		cache.push_front(load_from_path(path.path()));
	}
	return cache;
}

void update_playlist_items_cache(std::forward_list<PlaylistItems>& cache, std::shared_ptr<CURL> curl) {
	std::vector<std::string> plat_to_access_tkn(Platform::INVALID);
	for (int i = Platform::YOUTUBE; i != Platform::INVALID; i++) {
		Platform plat = static_cast<Platform>(i);
#ifndef NDEBUG 
		if (plat == Platform::TEST) continue; 
#endif
		plat_to_access_tkn[plat] = get_or_refresh_access_tkn(plat, curl);
	}

	auto prev = cache.before_begin();
	auto curr = cache.begin();
	while (curr != cache.end()) {
		int i = 0;
		while (i != curr->tracked.size()) {
			auto& [plat, pl] = curr->tracked[i];
			auto node = PlaylistCache::load_node(pl.id, plat);
			pl.title = node.playlist.title;

			const auto& access_tkn = plat_to_access_tkn[plat];
			Playlist modified_playlist;
			if (API::get_playlist(plat, curl.get(), access_tkn, pl.id, node.playlist.etag, modified_playlist)) {
				if (modified_playlist.id.empty()) {
					// playlist was deleted
					// TODO this should be saved in the node already
					node.playlist.short_id = pl.short_id;
					remove_node(node, plat);
					curr->tracked.erase(curr->tracked.begin() + i);
					curr->was_changed = true;
					continue;
				} else {
					pl.title = modified_playlist.title;
					node.playlist = std::move(modified_playlist);
					save_node(node, plat);
				}
			}
			i++;
		}

		if (curr->tracked.size() < 2) {
			curr = cache.erase_after(prev);
		} else {
			curr++;
			prev++;
		}
	}
}

void save_playlist_items_cache(const std::forward_list<PlaylistItems>& cache) {
	std::unordered_set<std::string> deleted_ids;
	for (const auto& e: fs::directory_iterator(dir())) {
		deleted_ids.insert(e.path().filename());
	}

	for (const auto& pl_items: cache) {
		deleted_ids.erase(pl_items.id);
		if (!pl_items.was_changed) {
			continue;
		}
		save_playlist_items(pl_items);	
	}
	
	for (const auto& id: deleted_ids) {
		remove_playlist_items(id);
	}
}

void save_playlist_items(const PlaylistItems& pl_items) {
	proto::PlaylistItems proto_items;
	for (const auto& [plat, pl]: pl_items.tracked) {
		auto proto_pair = proto_items.add_tracked();
		proto_pair->set_plat(get_proto_platform(plat));
		proto_pair->mutable_playlist()->set_id(pl.id);
		proto_pair->mutable_playlist()->set_items_etag(pl.items_etag);
	}
	for (const auto& song: pl_items.song_hashes) {
		proto_items.add_song_hashes(song);
	}
	auto file = ensure_bin_file<std::ofstream>(dir() / pl_items.id);
	proto_items.SerializeToOstream(&file);
}

void remove_playlist_items(const std::string& id) {
	auto items = get_proto_items(dir() / id);
	for (const auto& p: items.tracked()) {
		Platform plat = get_platform(p.plat());
		auto node = PlaylistCache::load_node(std::string(p.playlist().id()), plat);
		if (node.playlist.id.empty()) {
			continue;
		}
		node.items_id.clear();
		PlaylistCache::save_node(node, plat);
	}
	fs::remove(dir() / id);
}
