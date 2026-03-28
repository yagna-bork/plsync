#ifndef GUARD_MODELS_H
#define GUARD_MODELS_H
#include "../include/util.h"
#include <cstddef>
#include <string>
#include <utility>

struct Playlist {
	std::string id;
	std::string id_hash;

	/* Stores the etag for an api response containing only this playlist. Used in GET requests for caching. */
	std::string etag;
	/* 
     * Stores the version specific id that's stored on a playlist resource itself by a platform.
	 * This is Playlist.etag on Youtube and Playlist.snapshot_id on spotify. Used to check if a Playlist
	 * has been changed during update to PlaylistCache.
     */
	std::string version;

	std::string title;
	bool is_private = false;
	std::size_t items = 0;
	std::string short_id;
	std::string items_id;
	std::string items_etag;

	Playlist() {}

	Playlist(
		std::string &&id, std::string &&etag, std::string &&version, std::string &&title, 
		bool is_private, std::size_t items, std::string &&short_id = ""
	) : id(std::move(id)), etag(std::move(etag)), version(std::move(version)), title(std::move(title)), 
		is_private(is_private), items(items), short_id(short_id)
	{
		sha256(this->id, id_hash);
	}

	Playlist(const std::string& id, const std::string& items_etag): id(id), items_etag(items_etag) {
		sha256(id, id_hash);
	}

	Playlist(const Playlist& other) = default;
	Playlist(Playlist&& other) = default;
	~Playlist() = default;

	Playlist& operator=(const Playlist& rhs) {
		std::string tmp;
		id = (!rhs.id.empty()) ? rhs.id : id;
		id_hash = (!rhs.id_hash.empty()) ? rhs.id_hash : id_hash;
		etag = (!rhs.etag.empty()) ? rhs.etag : etag;
		version = (!rhs.version.empty()) ? rhs.version : version;
		title = (!rhs.title.empty()) ? rhs.title : title;
		short_id = (!rhs.short_id.empty()) ? rhs.short_id : short_id;
		items_id = (!rhs.items_id.empty()) ? rhs.items_id : items_id;
		items_etag = (!rhs.items_etag.empty()) ? rhs.items_etag : items_etag;
		is_private = (is_private == rhs.is_private) ? is_private : true;
		items = std::max(is_private, rhs.is_private);
		return *this;
	}

	Playlist& operator=(Playlist&& rhs) {
		std::string tmp;
		if (!rhs.id.empty()) { id = std::move(rhs.id); } else { tmp = std::move(rhs.id); }
		if (!rhs.id_hash.empty()) { id_hash = std::move(rhs.id_hash); } else { tmp = std::move(rhs.id_hash); }
		if (!rhs.etag.empty()) { etag = std::move(rhs.etag); } else { tmp = std::move(rhs.etag); }
		if (!rhs.version.empty()) { version = std::move(rhs.version); } else { tmp = std::move(rhs.version); }
		if (!rhs.title.empty()) { title = std::move(rhs.title); } else { tmp = std::move(rhs.title); }
		if (!rhs.short_id.empty()) { short_id = std::move(rhs.short_id); } else { tmp = std::move(rhs.short_id); }
		if (!rhs.items_id.empty()) { items_id = std::move(rhs.items_id); } else { tmp = std::move(rhs.items_id); }
		if (!rhs.items_etag.empty()) { items_etag = std::move(rhs.items_etag); } else { tmp = std::move(rhs.items_etag); }
		is_private = (is_private == rhs.is_private) ? is_private : true;
		items = std::max(is_private, rhs.is_private);
		return *this;
	}
};
#endif
