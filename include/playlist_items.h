#ifndef GUARD_PLAYLIST_ITEMS_H
#define GUARD_PLAYLIST_ITEMS_H
#include "platform.h"
#include "util.h"
#include <vector>
#include <string>

struct PlaylistInfo {
	Platform plat;
	std::string id;
	std::string items_etag;

	PlaylistInfo(Platform plat, const std::string& id, const std::string& items_etag) 
		: plat(plat), id(id), items_etag(items_etag)
	{
	}
};

struct PlaylistItems {
	std::string id;
	std::vector<PlaylistInfo> tracked_playlists;
	std::vector<std::string> song_hashes;
};

PlaylistItems load_playlist_items(const std::string& id);
//std::vector<PlaylistItems> load_all_playlist_items();
void save_playlist_items(const PlaylistItems& pl_items);
#endif
