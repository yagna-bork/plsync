#include "../include/playlist_items_cache.h"
#include "../include/playlist_items_cache.pb.h"
#include "../include/playlist_cache.h"
#include "../include/util.h"
#include "../include/config.h"
#include <cassert>
#include <ios>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace PlaylistItemsCache {

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
	for (const auto& proto_pl: proto_items.tracked_playlists()) {
		items.tracked_playlists.emplace_back(
			get_platform(proto_pl.plat()), 
			std::string(proto_pl.id()), 
			std::string(proto_pl.items_etag())
		);
	}
	for (const auto& song: proto_items.song_hashes()) {
		items.song_hashes.emplace_back(song);
	}
	items.id = path.filename();
	return items;
}

PlaylistItems load(const std::string& id) {
	return load_from_path(dir() / id);
}

std::vector<PlaylistItems> load_all() {
	std::vector<PlaylistItems> res;
	for (const auto& path: fs::directory_iterator(dir())) {
		res.push_back(load_from_path(path.path()));
	}
	return res;
}

void save(const PlaylistItems& items) {
	proto::PlaylistItems proto_items;
	for (auto& pl: items.tracked_playlists) {
		proto::PlaylistInfo* proto_pl = proto_items.add_tracked_playlists();
		proto_pl->set_plat(get_proto_platform(pl.plat));
		proto_pl->set_id(pl.id);
		proto_pl->set_items_etag(pl.items_etag);
	}
	for (const auto& song: items.song_hashes) {
		proto_items.add_song_hashes(song);
	}
	auto file = ensure_bin_file<std::ofstream>(dir() / items.id);
	proto_items.SerializeToOstream(&file);
}

void remove(const std::string& id) {
	auto items = get_proto_items(dir() / id);
	for (const auto& pl: items.tracked_playlists()) {
		Platform plat = get_platform(pl.plat());
		auto node = PlaylistCache::load_node(std::string(pl.id()), plat);
		node.items_id.clear();
		PlaylistCache::save_node(node, plat);
	}
	fs::remove(dir() / id);
}

} // PlaylistItemsCache
