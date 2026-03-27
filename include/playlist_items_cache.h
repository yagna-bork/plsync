#ifndef GUARD_PLAYLIST_ITEMS_H
#define GUARD_PLAYLIST_ITEMS_H
#include "platform.h"
#include "models.h"
#include "util.h"
#include <forward_list>
#include <string>
#include <utility>

struct PlaylistItems {
	std::string id;
	std::vector<std::pair<Platform, Playlist>> tracked;
	std::vector<std::string> song_hashes;
	bool was_changed;
};

std::forward_list<PlaylistItems> load_playlist_items_cache();
/* Can throw TokenStorageError and API::RequestError on failure */
void update_playlist_items_cache(std::forward_list<PlaylistItems>& cache);
void save_playlist_items_cache(const std::forward_list<PlaylistItems>& cache);

PlaylistItems load_playlist_items(const std::string& id);
void save_playlist_items(const PlaylistItems& pl_items);
void remove_playlist_items(const std::string& id);
#endif
