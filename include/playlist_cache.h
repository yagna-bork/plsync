#ifndef GUARD_PLAYLIST_CACHE_H 
#define GUARD_PLAYLIST_CACHE_H 
#include "models.h"
#include "platform.h"
#include <cstddef>
#include <vector>

struct CacheNode {
	Playlist playlist;
	bool is_tracked;
	CacheNode* next;
	bool was_changed;
	std::string id_hash;

	CacheNode() {}

	CacheNode(const std::string& id_hash, bool is_tracked = false)
		: was_changed(true), id_hash(id_hash), is_tracked(is_tracked)
	{
	}
};

struct CacheHead {
	CacheNode* next;
	bool was_changed;
};

struct PlaylistCache;

CacheHead* load_cache(Platform plat);
void update_cache(PlaylistCache& cache, const std::vector<Playlist>& playlists);
void update_cache(CacheHead* head, Platform plat, const std::vector<Playlist>& playlists);
std::size_t fill_short_ids(CacheHead* head);
void free_cache(CacheHead* head, Platform plat);

struct PlaylistCache {
	CacheHead* head;
	Platform plat;
	
	PlaylistCache(Platform plat): head(load_cache(plat)), plat(plat), owns_head(true) {}
	PlaylistCache(CacheHead* head, Platform plat): head(head), plat(plat), owns_head(false) {}
	~PlaylistCache() { if (owns_head) free_cache(head, plat); }

	void update(const std::vector<Playlist>& playlists) { update_cache(*this, playlists); }
	std::size_t fill_short_ids() { return ::fill_short_ids(head); }

	enum IteratorPos { HEAD, BETWEEN, END };

	template <bool is_const>
	class Iterator;
	using const_iterator = Iterator</*is_const=*/true>;
	using iterator = Iterator</*is_const=*/false>;

	const_iterator cbefore_begin() const;
	const_iterator cbegin() const;
	const_iterator cend() const;
	iterator before_begin();
	iterator begin();
	iterator end();
private:
	bool owns_head;
};

template <bool is_const>
class PlaylistCache::Iterator {
	friend void update_cache(PlaylistCache& cache, const std::vector<Playlist>& playlists);
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = std::conditional_t<is_const, const Playlist, Playlist>;
    using pointer           = std::conditional_t<is_const, const Playlist*, Playlist*>;
    using reference         = std::conditional_t<is_const, const Playlist&, Playlist&>;
	
	Iterator(): head(nullptr), node(nullptr), pos(IteratorPos::END) {}
	Iterator(CacheHead* head) : head(head), node(nullptr), pos(IteratorPos::HEAD) {}
	Iterator(const Iterator<false>& other): head(other.head), node(other.node), pos(other.pos) {}

	reference operator*() { if (!is_const) node->was_changed = true; return node->playlist; }
	pointer operator->() { if (!is_const) node->was_changed = true; return &node->playlist; }
	bool operator==(const Iterator& rhs) { return (pos != IteratorPos::BETWEEN && pos == rhs.pos) || node == rhs.node; }
	bool operator!=(const Iterator& rhs) { return !(*this == rhs); }

	Iterator& operator++() {
		node = next();
		pos = (node == nullptr) ? IteratorPos::END : IteratorPos::BETWEEN;
		return *this;
	}

	Iterator operator++(int) {
		auto tmp = *this;
		++*this;
		return tmp;
	}
private:
	CacheHead* head;
	CacheNode* node;
	IteratorPos pos;
	
	// TODO semantic decompress
	bool is_head() { return pos == IteratorPos::HEAD; }
	CacheNode* next() { return is_head() ? head->next : node->next; }

	void set_next(CacheNode* next) { 
		if (is_head()) { 
			head->next = next; 
			head->was_changed = true;
		} else { 
			node->next = next; 
			node->was_changed = true;
		} 
	}
};
#endif
