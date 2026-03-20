#ifndef GUARD_MODELS_H
#define GUARD_MODELS_H
#include <cstddef>
#include <string>
#include <utility>

struct Playlist {
	Playlist() {}

	Playlist(
		std::string &&id, std::string &&etag, std::string &&version, std::string &&title, 
		bool is_private, std::size_t items, std::string &&short_id = ""
	) : id(std::move(id)), etag(std::move(etag)), version(std::move(version)), title(std::move(title)), 
		is_private(is_private), items(items), short_id(short_id)
	{
	}

	std::string id;

	/* Stores the etag for an api response containing only this playlist. Used in GET requests for caching. */
	std::string etag;
	/* 
     * Stores the version specific id that's stored on a playlist resource itself by a platform.
	 * This is Playlist.etag on Youtube and Playlist.snapshot_id on spotify. Used to check if a Playlist
	 * has been changed during update to PlaylistCache.
     */
	std::string version;

	std::string title;
	bool is_private;
	std::size_t items;
	std::string short_id;
};
#endif
