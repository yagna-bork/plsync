#ifndef GUARD_PLAYLIST_CACHE_H 
#define GUARD_PLAYLIST_CACHE_H 
#include "models.h"
#include "platform.h"
#include <vector>

struct CacheNode {
	Playlist playlist;
	bool is_tracked;
	CacheNode* next;
	bool was_changed;
};

struct CacheHead {
	CacheNode* next;
	bool was_changed;
};

CacheHead* load_cache(Platform plat);
void update_cache(CacheHead* head, const std::vector<Playlist>& playlists);
void free_cache(CacheHead* head, Platform plat);

enum CacheIteratorPos { HEAD, BETWEEN, END };

template <bool is_const>
class CacheIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = std::conditional_t<is_const, const Playlist, Playlist>;
    using pointer           = std::conditional_t<is_const, const Playlist*, Playlist*>;
    using reference         = std::conditional_t<is_const, const Playlist&, Playlist&>;
	
	CacheIterator(): node(nullptr), pos(CacheIteratorPos::END) {}

	CacheIterator(CacheHead* head)
		: node(create_dummy_head(head->next)), pos(CacheIteratorPos::HEAD) 
	{}

	~CacheIterator() { 
		if (pos == CacheIteratorPos::HEAD) free_dummy_head(node); 
	}

	CacheIterator(const CacheIterator<false>& other): pos(other.pos) {
		node = (pos == CacheIteratorPos::HEAD) ? create_dummy_head(other.node->next) 
											  : other.node;
	}

	reference operator*() { 
		if (!is_const) node->was_changed = true; 
		return node->playlist; 
	}

	pointer operator->() { 
		if (!is_const) node->was_changed = true; 
		return &node->playlist; 
	}

	CacheIterator& operator++() {
		if (pos == CacheIteratorPos::HEAD) {
			auto next = node->next;
			free_dummy_head(node);
			node = next;
		} else {
			node = node->next;
		}
		pos = (node == nullptr) ? CacheIteratorPos::END : CacheIteratorPos::BETWEEN;
		return *this;
	}

	CacheIterator operator++(int) {
		auto tmp = *this;
		++*this;
		return tmp;
	}

	bool operator==(const CacheIterator& rhs) { 
		return node == rhs.node || (pos == CacheIteratorPos::HEAD && pos == rhs.pos); 
	}

	bool operator!=(const CacheIterator& rhs) { return !(*this == rhs); }
private:
	CacheNode* node;
	CacheIteratorPos pos;

	CacheNode* create_dummy_head(CacheNode* next) {
		auto dummy_head = new CacheNode; 
		dummy_head->next = next; 
		return dummy_head;
	}

	void free_dummy_head(CacheNode* dummy_head) { delete dummy_head; }
};

struct PlaylistCache {
	CacheHead* head;
	Platform plat;
	
	PlaylistCache(Platform plat): head(load_cache(plat)), plat(plat), owns_head(true) {}
	PlaylistCache(CacheHead* head, Platform plat): head(head), plat(plat), owns_head(false) {}
	~PlaylistCache() { if (owns_head) free_cache(head, plat); }

	// TODO call other update_cache
	void update(const std::vector<Playlist>& playlists) { update_cache(head, playlists); }

	using const_iterator = CacheIterator</*is_const=*/true>;
	using iterator = CacheIterator</*is_const=*/false>;

	const_iterator cbefore_begin() { return const_iterator(head); }
	const_iterator cbegin() { return ++const_iterator(head); }
	const_iterator cend() { return const_iterator(); }
	iterator before_begin() { return iterator(head); }
	iterator begin() { return ++iterator(head); }
	iterator end() { return iterator(); }
private:
	bool owns_head;
};
#endif
