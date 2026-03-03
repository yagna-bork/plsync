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
#include <stdexcept>
#include <type_traits>
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
	template <bool is_const>
	struct Iterator;

	using iterator = Iterator<false>;
	using const_iterator = Iterator<true>;

public:
	void update(const std::vector<Playlist> &playlists);
	std::vector<Playlist> get_playlists();
	std::vector<Playlist> get_playlists_sorted();

	~PlaylistCache();

protected:
	PlaylistCache(Platform platform, const std::string &name); 

private:
	/* forward_list interface begin */
	enum Position { HEAD, BETWEEN, END };

	const_iterator cbefore_begin();
	const_iterator cbegin();
	const_iterator cend();
	iterator before_begin();
	iterator begin();
	iterator end();

	template <bool is_const>
	Iterator<is_const> insert_after(Iterator<is_const>& pos, const Playlist& playlist);

	template <bool is_const>
	Iterator<is_const> erase_after(Iterator<is_const>& pos);

	template <bool is_const>
	void update_at(Iterator<is_const>& pos, const Playlist& playlist);
	/* forward_list interface end */

	static PlaylistCacheEntry get_entry(const Playlist &playlist);
	static Playlist get_playlist(const PlaylistCacheEntry& entry);

	void set_entry(PlaylistCacheEntry *entry, const Playlist &pl, bool set_id_hash = true);

	/* Get the name of the file where node is stored */
	inline std::string get_file_name(const PlaylistCacheNode &node) { 
		return absl::StrCat(node.entry().id(), ".pb");
	}

	inline void set_next(PlaylistCacheNode &node, const PlaylistCacheNode &next) { 
		node.set_next(get_file_name(next)); 
	}

	inline void read_node(const std::string_view &name, PlaylistCacheNode &node) {
		read_node_from_path(cache_dir/name, node);
	}

	/* Clears node if p doesn't exist */
	void read_node_from_path(std::filesystem::path p, PlaylistCacheNode &node);

	/* Saves the contents of node into the correct file */
	void save_node(const PlaylistCacheNode &node);

private:
	/*
  	 * This cache is a forward_list of cache files which are stored
	 * in cache_dir.
	 */
	std::filesystem::path cache_dir;

	/*
	 * The file pointing to the first
	 * element of the linked list
	 */
	std::filesystem::path head_path;

	bool is_sorted;
};

template <bool is_const>
struct PlaylistCache::Iterator {
	friend struct Iterator<!is_const>;

	using iterator_category = std::forward_iterator_tag;
	using value_type = std::conditional_t<is_const, const PlaylistCacheEntry, PlaylistCacheEntry>;
	using difference_type = std::ptrdiff_t;
	using pointer = std::conditional_t<is_const, const PlaylistCacheEntry*, PlaylistCacheEntry*>;
	using reference = std::conditional_t<is_const, const PlaylistCacheEntry&, PlaylistCacheEntry&>;

	std::filesystem::path cache_dir;
	std::shared_ptr<std::fstream> file;
	std::shared_ptr<PlaylistCacheNode> node;
	Position pos;
	bool was_changed;

	Iterator(const std::filesystem::path &cache_dir, Position pos, const std::string &name = "");

	/* RULE OF 5 */
	template <bool is_other_const>
	Iterator(const Iterator<is_other_const> &other);

	template <bool is_rhs_const>
	Iterator<is_const>& operator=(const Iterator<is_rhs_const> &rhs);

	template <bool is_other_const>
	Iterator(Iterator<is_other_const> &&other);

	template <bool is_rhs_const>
	Iterator<is_const>& operator=(Iterator<is_rhs_const> &&rhs);

	~Iterator() { save(); }


	/* Iterator interface */
	reference operator*();
	pointer operator->();
	bool operator==(const Iterator<is_const> &rhs) const;
	bool operator!=(const Iterator<is_const> &rhs) const;
	Iterator<is_const>& operator++();
	Iterator<is_const> operator++(int);


	/* Other */
	void save();
	std::string next() const;

	/* 
	 * Changing next doesn't violate the const iterator requirement
	 * because next is a member of PlaylistCacheNode, not PlaylistCacheEntry,
	 * which is not the class that const requirements applies to.
	 */
	void set_next(std::string fname);
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
