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

class MetaCache: public Cache {
public:
	void update(const std::vector<Playlist> &playlists);

	std::vector<Playlist> get_playlists();

	std::vector<Playlist> get_playlists_sorted();

protected:
	MetaCache(Platform platform, const std::string &name); 

private:
	void set_entry(MetaCacheEntry *entry, const Playlist &pl, bool set_id_hash = true);
	
	/* Get the name of the file where node is stored */
	inline std::string get_file_name(const MetaCacheNode &node) { 
		return absl::StrCat(node.entry().id(), ".pb");
	}

	inline void set_next(MetaCacheNode &node, const MetaCacheNode &next) { 
		node.set_next(get_file_name(next)); 
	}

	inline void set_prev(MetaCacheNode &node, const MetaCacheNode &prev) { 
		node.set_prev(get_file_name(prev)); 
	}

	inline void read_node(const std::string_view &name, MetaCacheNode &node) {
		read_node_from_path(subdir/name, node);
	}

	/* Clears node if p doesn't exist */
	void read_node_from_path(std::filesystem::path p, MetaCacheNode &node);

	/* Saves the contents of node into the correct file */
	void save_node(const MetaCacheNode &node);

	/* Persist whether cache is sorted by title in ascending order or not */
	void set_is_sorted(bool val);

	bool is_sorted();
	
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
};

class UntrackedCache : public MetaCache {
public:
	UntrackedCache(Platform platform) : MetaCache(platform, "untracked") {}
};

class PlaylistCacheDiff {
};

class PlaylistCache {
};
#endif
