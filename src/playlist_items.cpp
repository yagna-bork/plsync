#include "../include/playlist_items.h"
#include "../include/playlist_items.pb.h"
#include "../include/util.h"
#include "../include/config.h"
#include <ios>
#include <filesystem>
#include <fstream>

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

PlaylistItems load_playlist_items(const std::string& id) {
	PlaylistItems items;
	proto::PlaylistItems proto_items;
	{
		auto file = ensure_bin_file<std::ifstream>(dir() / id);
		proto_items.ParseFromIstream(&file);
	}
	for (const auto& proto_pl: proto_items.tracked_playlists()) {
		items.tracked_playlists.emplace_back(
			get_platform(proto_pl.plat()), std::string(proto_pl.id()), std::string(proto_pl.items_etag())
		);
	}
	for (const auto& song: proto_items.song_hashes()) {
		items.song_hashes.emplace_back(song);
	}
	return items;
}

void save_playlist_items(const PlaylistItems& items) {
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
