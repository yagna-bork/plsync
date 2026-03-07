#ifndef GUARD_PLAYLIST_CACHE_H
#define GUARD_PLAYLIST_CACHE_H 
#include "models.h"
#include "platform.h"
#include <cassert>
#include <cstddef>
#include <vector>

namespace PlaylistCache {

struct Node {
	Playlist playlist;
	bool is_tracked;
	Node* next;
	bool was_changed;
	std::string id_hash;

	Node() {}

	Node(const std::string& id_hash, bool is_tracked)
		: was_changed(true), id_hash(id_hash), is_tracked(is_tracked)
	{
	}
};

struct Head {
	Node* next;
	bool was_changed;
	std::string etag;
	std::size_t sid_len;
};

Head* load(Platform plat);
void cleanup(Head* head, Platform plat);
void update(Head* head, Platform plat, const std::vector<Playlist>& playlists, const std::string& etag);

/* Determine min characters of id_hash that make them all unique */
std::size_t calculate_short_id_len(Head* head);
void fill_short_ids(Head* head, std::size_t short_id_len);


struct Handle {
	Head* head;
	Platform plat;
	
	Handle(Platform plat): head(load(plat)), plat(plat) {}
	~Handle() { cleanup(head, plat);  }
};

enum IteratorPos { HEAD, BETWEEN, END };

template <bool is_const>
struct Iterator {
	union Ptr { Head* head; Node* node; };
	Ptr ptr;
	IteratorPos pos;

    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = std::conditional_t<is_const, const Playlist, Playlist>;
    using pointer           = std::conditional_t<is_const, const Playlist*, Playlist*>;
    using reference         = std::conditional_t<is_const, const Playlist&, Playlist&>;
	
	Iterator(): pos(IteratorPos::END) {}
	Iterator(Head* head) : pos(IteratorPos::HEAD) { ptr.head = head; }

	reference operator*() { if (!is_const) ptr.node->was_changed = true; return ptr.node->playlist; }
	pointer operator->() { if (!is_const) ptr.node->was_changed = true; return &ptr.node->playlist; }

	bool operator==(const Iterator& rhs) {
		if (pos != rhs.pos) {
			return false;
		} else if (pos != IteratorPos::BETWEEN) {
			return true;
		} else {
			return ptr.node == rhs.ptr.node;
		}
	}

	bool operator!=(const Iterator& rhs) { return !(*this == rhs); }

	Iterator& operator++() {
		ptr.node = (pos == IteratorPos::HEAD) ? ptr.head->next : ptr.node->next;
		pos = (ptr.node == nullptr) ? IteratorPos::END : IteratorPos::BETWEEN;
		return *this;
	}

	Iterator operator++(int) {
		auto tmp = *this;
		++*this;
		return tmp;
	}
};

using const_iterator = Iterator</*is_const=*/true>;
using iterator = Iterator</*is_const=*/false>;

const_iterator cbefore_begin(Head* head);
const_iterator cbegin(Head* head);
const_iterator cend();
iterator before_begin(Head* head);
iterator begin(Head* head);
iterator end();
};
#endif
