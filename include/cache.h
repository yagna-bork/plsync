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
	const_iterator cbefore_begin();
	const_iterator cbegin();
	const_iterator cend();
	iterator before_begin();
	iterator begin();
	iterator end();

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
	using iterator_catagory = std::forward_iterator_tag;
	using value_type = std::conditional_t<is_const, const PlaylistCacheNode, PlaylistCacheNode>;
	using difference_type = std::ptrdiff_t;
	using pointer = std::conditional_t<is_const, const PlaylistCacheNode*, PlaylistCacheNode*>;
	using reference = std::conditional_t<is_const, const PlaylistCacheNode&, PlaylistCacheNode&>;

	enum Position { HEAD, BETWEEN, END  };
	
	const std::filesystem::path cache_dir;
	std::shared_ptr<std::fstream> file;
	std::shared_ptr<PlaylistCacheNode> node;
	Position pos;
	bool was_changed;

	Iterator(
		const std::filesystem::path &cache_dir, Position pos, const std::string &name = ""
	) : cache_dir(cache_dir), pos(pos), was_changed(false)
	{
		if (pos == Position::END) return;
		file = std::make_shared<std::fstream>(cache_dir / name, std::ios::binary);
		node->ParseFromIstream(file.get());
	}

	~Iterator() {
		if (pos != Position::BETWEEN || !was_changed) return;
		file->seekp(0);
		node->SerializeToOstream(file.get());
	}

	reference operator*() { 
		// assume that a change was made whenever a non-const iterator is dereferenced
		was_changed = !std::is_const_v<reference>; 
		return *node.get(); 
	}

	pointer operator->() { 
		// assume that a change was made whenever a non-const iterator is dereferenced
		was_changed = !std::is_const_v<pointer>; 
		return node.get(); 
	}

	bool operator==(const Iterator<is_const> &rhs) const { 
		if (pos != Position::BETWEEN) return pos == rhs.pos;
		return node->entry().id() == rhs.node->entry().id();
	}

	bool operator!=(const Iterator<is_const> &rhs) const {
		return !(*this == rhs);
	}

	Iterator<is_const>& operator++() {
		if (pos == Position::END) return *this; // undefined
		if (pos == Position::BETWEEN && was_changed) {
			file->seekp(0);
			node->SerializeToOstream(file.get());
		}
		if (node->next().empty()) {
			file.reset();
			node->Clear();
			pos = Position::END;
			return *this;
		}
		file = std::make_shared<std::fstream>(cache_dir/node->next(), std::ios::binary);
		node->ParseFromIstream(file.get());
		pos = Position::BETWEEN;
		return *this;
	}

	Iterator<is_const> operator++(int) {
		auto tmp = *this;
		++this;
		return tmp;
	}
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
