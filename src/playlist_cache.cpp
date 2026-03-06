#include "../include/playlist_cache.h"
#include "../include/models.h"
#include "../include/config.h"
#include "../include/playlist_cache.pb.h"
#include "../include/util.h"
#include <cassert>
#include <vector>
#include <filesystem>
#include <ios>
#include <fstream>
#include <deque>
#include <unordered_map>

namespace fs = std::filesystem;

namespace PlaylistCache {

inline fs::path dir(Platform plat) { 
	fs::path cache_dir = get_setting("cache_dir");
	return cache_dir / "playlist" / title_lower(plat); 
}

inline fs::path head_path(Platform plat) { return dir(plat) / "HEAD"; }

inline fs::path node_path(Node* node, Platform plat) {
	assert(node && !node->playlist.id.empty());
	return dir(plat) / node->playlist.id;
}

inline void set_next(proto::CacheHead &proto_head, Node* node) {
	std::string next = node ? node->playlist.id : "";
	proto_head.set_next(next);
}

inline void set_next(proto::CacheNode &proto_node, Node* node) {
	std::string next = node ? node->playlist.id : "";
	proto_node.set_next(next);
}

void set_next(const_iterator it, Node* next) {
	if (it.pos == IteratorPos::HEAD) {
		it.ptr.head->next = next;
		it.ptr.head->was_changed = true;
	} else {
		it.ptr.node->next = next;
		it.ptr.node->was_changed = true;
	}
}

inline void free_and_advance_node(Node** ptr) {
	Node* tmp = *ptr;
	*ptr = (*ptr)->next;
	delete tmp;
}

void update_playlist(Playlist& pl, const Playlist& other_pl) {
	pl.id = other_pl.id;
	pl.etag = other_pl.etag;
	pl.title = other_pl.title;
	pl.is_private = other_pl.is_private;
	pl.items = other_pl.items;
}

const_iterator cbefore_begin(Head* head) { return const_iterator(head); }
const_iterator cbegin(Head* head) { return ++const_iterator(head); }
const_iterator cend() { return const_iterator(); }
iterator before_begin(Head* head) { return iterator(head); }
iterator begin(Head* head) { return ++iterator(head); }
iterator end() { return iterator(); }

Head* load(Platform plat) { 
	// ensure HEAD file and parent directory exist
	fs::create_directories(dir(plat));
	if (!fs::exists(head_path(plat))) {
		std::ofstream f(head_path(plat), std::ios::binary);
	}

	proto::CacheHead proto_head;
	{
		std::ifstream f(head_path(plat), std::ios::binary);
		proto_head.ParseFromIstream(&f);
	}
	Head* head = new Head;
	head->etag = proto_head.etag();
	std::string next_id(proto_head.next());
	if (next_id.empty()) {
		return head;
	}

	Node** next_ptr = &head->next;
	while (!next_id.empty()) {
		proto::CacheNode proto_node;
		std::ifstream f(dir(plat) / next_id, std::ios::binary);
		proto_node.ParseFromIstream(&f);
		const auto& proto_pl = proto_node.playlist();

		auto new_node = new Node(std::string(proto_node.id_hash()), proto_node.is_tracked());
		auto& node_pl = new_node->playlist;
		node_pl.id = std::string(proto_pl.id());
		node_pl.etag = std::string(proto_pl.etag());
		node_pl.title = std::string(proto_pl.title());
		node_pl.is_private = proto_pl.is_private();
		node_pl.items = proto_pl.items();
		
		*next_ptr = new_node;
		next_ptr = &new_node->next;
		next_id = std::string(proto_node.next());
	}
	return head;
}

void cleanup(Head* head, Platform plat) {
	// save and delete head
	if (head->was_changed) {
		proto::CacheHead proto_head;
		set_next(proto_head, head->next);
		proto_head.set_etag(head->etag);
		std::ofstream f(head_path(plat), std::ios::binary);
		proto_head.SerializeToOstream(&f);
	}
	Node* node = head->next;
	delete head;

	// save nodes
	while (node) {
		if (!node->was_changed) {
			free_and_advance_node(&node);
			continue;
		}
		proto::CacheNode proto_node;
		set_next(proto_node, node->next);
		proto_node.set_is_tracked(node->is_tracked);
		proto_node.set_id_hash(node->id_hash);

		auto proto_pl = proto_node.mutable_playlist();
		const auto& node_pl = node->playlist;
		proto_pl->set_id(node_pl.id);
		proto_pl->set_etag(node_pl.etag);
		proto_pl->set_title(node_pl.title);
		proto_pl->set_is_private(node_pl.is_private);
		proto_pl->set_items(node_pl.items);

		std::ofstream f(node_path(node, plat), std::ios::binary);
		proto_node.SerializeToOstream(&f);
		free_and_advance_node(&node);
	}
}

void update(Head* head, Platform plat, const std::vector<Playlist>& playlists, const std::string& etag) {
	std::size_t n = playlists.size();
	std::deque<bool> is_new(n, true);
	std::unordered_map<std::string, std::size_t> etag_to_idx;
	std::unordered_map<std::string, std::size_t> id_to_idx;
	for (std::size_t i = 0; i != n; i++) {
		etag_to_idx[playlists[i].etag] = i;
		id_to_idx[playlists[i].id] = i;
	}

	head->etag = etag;
	head->was_changed = true;

	auto prev = cbefore_begin(head);
	auto curr = cbegin(head);
	auto curr_write = begin(head);
	while (curr != cend()) {
		// case 1: etag unchanged, playlist unchanged
		std::string etag(curr->etag);
		if (etag_to_idx.count(etag)) {
			std::size_t i = etag_to_idx[etag];
			is_new[i] = false;
			prev++; curr++; curr_write++;
			continue;
		}

		// case 2: etag changed but id found, playlist changed
		std::string id(curr->id);
		if (id_to_idx.count(id)) {
			std::size_t i = id_to_idx[id];
			is_new[i] = false;
			update_playlist(*curr_write, playlists[i]);
			prev++; curr++; curr_write++;
			continue;
		}
		
		// case 3: id not found, playlist deleted
		Node* tmp = curr.ptr.node;
		curr++; curr_write++;
		set_next(prev, tmp->next);
		fs::remove(node_path(tmp, plat));
		delete tmp;
	}

	// case 4: playlist not found in cache, new playlist
	curr = prev;
	for (std::size_t i = 0; i != n; i++) {
		if (!is_new[i]) {
			continue;
		}
		std::string id_hash;
		sha256(playlists[i].id, id_hash);
		Node* new_node = new Node(id_hash, false);
		update_playlist(new_node->playlist, playlists[i]);

		set_next(curr, new_node);
		curr++;
	}
}

std::size_t fill_short_ids(Head* head) {
	std::vector<std::string> id_hashes;
	Node* node = head->next;
	while (node) {
		id_hashes.emplace_back(node->id_hash);
		node = node->next;
	}

	// determine min length of short_id (sid) to make all unique
	std::vector<std::size_t> collision_idxs(id_hashes.size());
	std::iota(collision_idxs.begin(), collision_idxs.end(), 0);

	std::unordered_map<std::string, std::vector<std::size_t>> sid_groups;
	std::size_t sid_len = 0;
	while (!collision_idxs.empty()) {
		sid_len++;
		sid_groups.clear();
		for (std::size_t idx: collision_idxs) {
			std::string sid(id_hashes[idx].data(), sid_len);
			sid_groups[sid].push_back(idx);
		}
		
		collision_idxs.clear();
		for (const auto &pr: sid_groups) {
			const auto &group = pr.second;
			if (group.size() < 2) {
				continue;
			}
			std::copy(group.begin(), group.end(), std::back_inserter(collision_idxs));
		}
	}

	node = head->next;
	while (node) {
		std::string short_id(node->id_hash.data(), sid_len);
		node->playlist.short_id = short_id;
		node = node->next;
	}
	return sid_len;
}

}
