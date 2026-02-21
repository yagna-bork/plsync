#ifndef GUARD_MODELS_H
#define GUARD_MODELS_H
#include <cstddef>
#include <string>
#include <utility>

struct Playlist {
	Playlist() {}

	Playlist(
		std::string &&id, std::string &&etag, std::string &&title, bool is_private, 
		std::size_t items, std::string &&short_id = ""
	) : id(std::move(id)), etag(std::move(etag)), title(std::move(title)), 
		is_private(is_private), items(items), short_id(short_id)
	{
	}

	std::string id;
	std::string etag;
	std::string title;
	bool is_private;
	std::size_t items;
	std::string short_id;
};
#endif
