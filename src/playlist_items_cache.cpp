#include "../include/playlist_items_cache.h"
#include "../include/playlist_items_cache.pb.h"
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

PlaylistItems load(const std::string& id) {
	return load(dir() / id);
}

PlaylistItems load(const fs::path& path) {
	PlaylistItems items;
	proto::PlaylistItems proto_items;
	{
		auto file = ensure_bin_file<std::ifstream>(path);
		proto_items.ParseFromIstream(&file);
	}
	for (const auto& proto_pl: proto_items.tracked_playlists()) {
		items.tracked_playlists.emplace_back(
			get_platform(proto_pl.plat()), 
			std::string(proto_pl.id()), 
			std::string(proto_pl.items_etag()),
			std::string(proto_pl.title())
		);
	}
	for (const auto& song: proto_items.song_hashes()) {
		items.song_hashes.emplace_back(song);
	}
	return items;
}

std::vector<PlaylistItems> load_all() {
	std::vector<PlaylistItems> res;
	for (const auto& path: fs::directory_iterator(dir())) {
		res.push_back(load(path.path()));
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
		proto_pl->set_title(pl.title);
	}
	for (const auto& song: items.song_hashes) {
		proto_items.add_song_hashes(song);
	}
	auto file = ensure_bin_file<std::ofstream>(dir() / items.id);
	proto_items.SerializeToOstream(&file);
}

void update_title(const std::string& title, const std::string& items_id, Platform plat) {
	proto::PlaylistItems proto_items;
	auto file = ensure_bin_file<std::fstream>(dir() / items_id, std::ios::in | std::ios::out);
	proto_items.ParseFromIstream(&file);
	for (auto& pl: *proto_items.mutable_tracked_playlists()) {
		if (pl.plat() == get_proto_platform(plat)) {
			pl.set_title(title);
			proto_items.SerializeToOstream(&file);
			return;
		}
	}
	assert(false); // plat is not being tracked by this PlaylistItems
}

} // PlaylistItemsCache
