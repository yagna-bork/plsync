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
#include "absl/strings/string_view.h"
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
	// std::vector<Playlist> get_playlists();

	// std::vector<Playlist> get_playlists_sorted();

	void update(const std::vector<Playlist> &playlists);

protected:
	MetaCache(Platform platform, const std::string &name); 

private:
	void set_entry(MetaCacheEntry *entry, const Playlist &pl);
	
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

	inline MetaCacheNode read_node(const std::string &name) {
		return read_node(subdir / name);
	}

	MetaCacheNode read_node(std::filesystem::path p);

	/* Saves the contents of node into the correct file */
	void save_node(const MetaCacheNode &node);
	
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
	std::filesystem::path head;
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
