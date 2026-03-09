#ifndef GUARD_PLAYLIST_CACHE_H
#define GUARD_PLAYLIST_CACHE_H 
#include "models.h"
#include "platform.h"
#include <cassert>
#include <cstddef>
#include <vector>

namespace PlaylistCache {

struct Node;
struct Head;

struct PtrUnion {
	bool is_head;
	union {
		Head* head; 
		Node* node; 
	};
};

struct Node {
	Playlist playlist;
	std::string items_id;
	Node* next;
	PtrUnion prev;
	bool was_changed;
	std::string id_hash;

	Node() {}

	Node(const std::string& id_hash, const std::string& items_id, bool was_changed)
		: id_hash(id_hash), items_id(items_id), was_changed(was_changed)
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
void update(Head* head, Platform plat, const std::vector<Playlist>& playlists, const std::string& etag);
void cleanup(Head* head, Platform plat);

/* Providing an invalid id is undefined behaviour */
Node load_node(const std::string& id, Platform plat); 
void update_node(Node& node, Platform plat, const Playlist& playlist); 
void delete_node(Node& id, Platform plat); 

/* Determine min characters of id_hash that make them all unique */
std::size_t calculate_short_id_len(Head* head);
void fill_short_ids(Head* head, std::size_t short_id_len);

template <bool is_const>
struct Iterator {
	PtrUnion ptr;

    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = std::conditional_t<is_const, const Playlist, Playlist>;
    using pointer           = std::conditional_t<is_const, const Playlist*, Playlist*>;
    using reference         = std::conditional_t<is_const, const Playlist&, Playlist&>;
	
	Iterator() { ptr.is_head = false; ptr.node = nullptr; }
	Iterator(Head* head) { ptr.is_head = true; ptr.head = head; }

	reference operator*() { if (!is_const) ptr.node->was_changed = true; return ptr.node->playlist; }
	pointer operator->() { if (!is_const) ptr.node->was_changed = true; return &ptr.node->playlist; }

	bool operator==(const Iterator& rhs) {
		if (ptr.is_head || rhs.ptr.is_head) {
			return ptr.is_head && rhs.ptr.is_head;
		}  else {
			return ptr.node == rhs.ptr.node;
		}
	}

	bool operator!=(const Iterator& rhs) { return !(*this == rhs); }

	Iterator& operator++() {
		ptr.node = (ptr.is_head) ? ptr.head->next : ptr.node->next;
		ptr.is_head = false;
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

struct Handle {
	Head* head;
	Platform plat;
	
	Handle(Platform plat): head(load(plat)), plat(plat) {}
	~Handle() { cleanup(head, plat);  }

	const_iterator cbefore_begin();
	const_iterator cbegin();
	const_iterator cend();
	iterator before_begin();
	iterator begin();
	iterator end();
};

} // namespace PlaylistCache
#endif
