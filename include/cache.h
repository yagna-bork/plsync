#ifndef GUARD_CACHE_H
#define GUARD_CACHE_H
#include "models.h"
#include "platform.h"
#include "config.h"
#include "cache.pb.h"
#include <vector>
#include <filesystem>
#include <ios>
#include <fstream>
#include <string_view>
#include "absl/strings/str_cat.h"

class Cache {
protected:
	Cache(Platform platform) 
		: platform(platform), 
		  parent_dir(
			  std::filesystem::path(get_setting("cache_dir")) / title_lower(platform)
		  )
	{
		std::filesystem::create_directories(parent_dir);
	}

protected:
	Platform platform;
	std::filesystem::path parent_dir;
};

class PlaylistCache: public Cache {
public:
	void update(const std::vector<Playlist> &playlists);

	std::vector<Playlist> get_playlists();

	std::vector<Playlist> get_playlists_sorted();

	~PlaylistCache();

protected:
	PlaylistCache(Platform platform, const std::string &name); 

private:
	void set_entry(PlaylistCacheEntry *entry, const Playlist &pl, bool set_id_hash = true);
	
	/* Get the name of the file where node is stored */
	inline std::string get_file_name(const PlaylistCacheNode &node) { 
		return absl::StrCat(node.entry().id(), ".pb");
	}

	inline void set_next(PlaylistCacheNode &node, const PlaylistCacheNode &next) { 
		node.set_next(get_file_name(next)); 
	}

	inline void read_node(const std::string_view &name, PlaylistCacheNode &node) {
		read_node_from_path(subdir/name, node);
	}

	/* Clears node if p doesn't exist */
	void read_node_from_path(std::filesystem::path p, PlaylistCacheNode &node);

	/* Saves the contents of node into the correct file */
	void save_node(const PlaylistCacheNode &node);
	
private:
	/*
  	 * This cache is a linked list of
	 * cache files which are stored
	 * under subdir with name `name`
	 */
	std::filesystem::path subdir;
	
	/*
	 * The file pointing to the first
	 * element of the linked list
	 */
	std::filesystem::path head_path;

	bool is_sorted;
};

class UntrackedCache : public PlaylistCache {
public:
	UntrackedCache(Platform platform) : PlaylistCache(platform, "untracked") {}
};

class PlaylistItemsDiff {
};

class PlaylistItemsCache {
};
#endif
