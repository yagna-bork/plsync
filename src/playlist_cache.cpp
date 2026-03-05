#include "../include/playlist_cache.h"
#include "../include/models.h"
#include "../include/config.h"
#include "../include/playlist_cache.pb.h"
#include <cassert>
#include <vector>
#include <filesystem>
#include <ios>
#include <fstream>
#include <deque>
#include <unordered_map>

namespace fs = std::filesystem;

inline fs::path dir(Platform plat) { 
	fs::path cache_dir = get_setting("cache_dir");
	return cache_dir / "playlist" / title_lower(plat); 
}

inline fs::path head_path(Platform plat) { return dir(plat) / "HEAD"; }

inline fs::path node_path(CacheNode* node, Platform plat) {
	assert(node && !node->playlist.id.empty());
	return dir(plat) / node->playlist.id;
}

inline void set_next(CacheHead* head, CacheNode* node) {
	assert(head != nullptr);
	head->next = node;
	head->was_changed = true;
}

inline void set_next(CacheNode* node, CacheNode* next) {
	assert(node != nullptr);
	node->next = next;
	node->was_changed = true;
}

inline void set_proto_next(proto::CacheHead &proto_head, CacheNode* node) {
	std::string next = node ? node->playlist.id : "";
	proto_head.set_next(next);
}

inline void set_proto_next(proto::CacheNode &proto_node, CacheNode* node) {
	std::string next = node ? node->playlist.id : "";
	proto_node.set_next(next);
}

inline void advance_node(CacheNode** ptr) { 
	assert(ptr && *ptr);
	*ptr = (*ptr)->next; 
}

inline void free_and_advance_node(CacheNode** ptr) {
	CacheNode* tmp = *ptr;
	advance_node(ptr);
	delete tmp;
}

CacheHead* load_cache(Platform plat) { 
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
	CacheHead* head = new CacheHead;
	std::string next_id(proto_head.next());
	if (next_id.empty()) {
		return head;
	}

	auto dummy_node = new CacheNode;
	auto node = dummy_node;
	while (!next_id.empty()) {
		set_next(node, new CacheNode);
		advance_node(&node);
		proto::CacheNode proto_node;
		std::ifstream f(dir(plat) / next_id, std::ios::binary);
		proto_node.ParseFromIstream(&f);

		node->was_changed = false;
		node->is_tracked = proto_node.is_tracked();

		auto& node_pl = node->playlist;
		const auto& proto_pl = proto_node.playlist();
		node_pl.id = std::string(proto_pl.id());
		node_pl.etag = std::string(proto_pl.etag());
		node_pl.title = std::string(proto_pl.title());
		node_pl.is_private = proto_pl.is_private();
		node_pl.items = proto_pl.items();

		next_id = std::string(proto_node.next());
	}
	set_next(head, dummy_node->next);
	delete dummy_node;
	return head;
}

void free_cache(CacheHead* head, Platform plat) {
	// save and delete head
	if (head->was_changed) {
		proto::CacheHead proto_head;
		set_proto_next(proto_head, head->next);
		std::ofstream f(head_path(plat), std::ios::binary);
		proto_head.SerializeToOstream(&f);
	}
	CacheNode* node = head->next;
	delete head;

	// save nodes
	while (node) {
		if (!node->was_changed) {
			free_and_advance_node(&node);
			continue;
		}
		proto::CacheNode proto_node;
		set_proto_next(proto_node, node->next);
		proto_node.set_is_tracked(node->is_tracked);

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

void update_playlist(Playlist& pl, const Playlist& other_pl) {
	pl.id = other_pl.id;
	pl.etag = other_pl.etag;
	pl.title = other_pl.title;
	pl.is_private = other_pl.is_private;
	pl.items = other_pl.items;
}

void update_node_playlist(CacheNode* node, const Playlist& other_pl) {
	assert(node != nullptr);
	update_playlist(node->playlist, other_pl);
	node->was_changed = true;
}

void update_cache(CacheHead* head, const std::vector<Playlist>& playlists) {
	std::size_t n = playlists.size();
	std::deque<bool> is_new(n, true);
	std::unordered_map<std::string, std::size_t> etag_to_idx;
	std::unordered_map<std::string, std::size_t> id_to_idx;
	for (std::size_t i = 0; i != n; i++) {
		etag_to_idx[playlists[i].etag] = i;
		id_to_idx[playlists[i].id] = i;
	}

	// TODO compress this dummy_node nonsense
	CacheNode* dummy_node = new CacheNode;
	dummy_node->next = head->next;
	CacheNode* prev = dummy_node;
	CacheNode* curr = dummy_node->next;
	while (curr) {
		// case 1: etag unchanged, playlist unchanged
		std::string etag(curr->playlist.etag);
		if (etag_to_idx.count(etag)) {
			std::size_t i = etag_to_idx[etag];
			is_new[i] = false;
			advance_node(&curr);
			advance_node(&prev);
			continue;
		}

		// case 2: etag changed but id found, playlist changed
		std::string id(curr->playlist.id);
		if (id_to_idx.count(id)) {
			std::size_t i = id_to_idx[id];
			is_new[i] = false;
			update_node_playlist(curr, playlists[i]);
			advance_node(&curr);
			advance_node(&prev);
			continue;
		}
		
		// case 3: id not found, playlist deleted
		set_next(prev, curr->next);
		free_and_advance_node(&curr);
	}

	// case 4: playlist not found in cache, new playlist
	CacheNode* new_node = prev;
	for (std::size_t i = 0; i != n; i++) {
		if (!is_new[i]) continue;
		set_next(new_node, new CacheNode);
		advance_node(&new_node);
		new_node->is_tracked = false;
		update_node_playlist(new_node, playlists[i]);
	}
	if (head->next != dummy_node->next) {
		set_next(head, dummy_node->next);
	}
	delete dummy_node;
}
