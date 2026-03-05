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

inline fs::path dir(Platform plat) { 
	fs::path cache_dir = get_setting("cache_dir");
	return cache_dir / "playlist" / title_lower(plat); 
}

inline fs::path head_path(Platform plat) { return dir(plat) / "HEAD"; }

inline fs::path node_path(CacheNode* node, Platform plat) {
	assert(node && !node->playlist.id.empty());
	return dir(plat) / node->playlist.id;
}

inline void set_proto_next(proto::CacheHead &proto_head, CacheNode* node) {
	std::string next = node ? node->playlist.id : "";
	proto_head.set_next(next);
}

inline void set_proto_next(proto::CacheNode &proto_node, CacheNode* node) {
	std::string next = node ? node->playlist.id : "";
	proto_node.set_next(next);
}

inline void free_and_advance_node(CacheNode** ptr) {
	CacheNode* tmp = *ptr;
	*ptr = (*ptr)->next;
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
		proto::CacheNode proto_node;
		std::ifstream f(dir(plat) / next_id, std::ios::binary);
		proto_node.ParseFromIstream(&f);
		next_id = std::string(proto_node.next());

		node->next = new CacheNode(std::string(proto_node.id_hash()), proto_node.is_tracked());
		node = node->next;

		const auto& proto_pl = proto_node.playlist();
		auto& node_pl = node->playlist;
		node_pl.id = std::string(proto_pl.id());
		node_pl.etag = std::string(proto_pl.etag());
		node_pl.title = std::string(proto_pl.title());
		node_pl.is_private = proto_pl.is_private();
		node_pl.items = proto_pl.items();
	}
	head->next = node;
	head->was_changed = true;
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

void update_cache(PlaylistCache& cache, const std::vector<Playlist>& playlists) {
	std::size_t n = playlists.size();
	std::deque<bool> is_new(n, true);
	std::unordered_map<std::string, std::size_t> etag_to_idx;
	std::unordered_map<std::string, std::size_t> id_to_idx;
	for (std::size_t i = 0; i != n; i++) {
		etag_to_idx[playlists[i].etag] = i;
		id_to_idx[playlists[i].id] = i;
	}

	auto prev = cache.cbefore_begin();
	auto curr = cache.cbegin();
	auto curr_write = cache.begin();
	while (curr != cache.cend()) {
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
		CacheNode* tmp = prev.next();
		prev.set_next(curr.next());
		fs::remove(node_path(tmp, cache.plat));
		delete tmp;
		curr++; curr_write++;
	}

	// case 4: playlist not found in cache, new playlist
	curr = prev;
	for (std::size_t i = 0; i != n; i++) {
		if (!is_new[i]) {
			continue;
		}
	
		std::string id_hash;
		sha256(playlists[i].id, id_hash);
		CacheNode* new_node = new CacheNode(id_hash);
		update_playlist(new_node->playlist, playlists[i]);
		curr.set_next(new_node);
		curr++;
	}
}

void update_cache(CacheHead* head, Platform plat, const std::vector<Playlist>& playlists) {
	PlaylistCache cache(head, plat);
	update_cache(cache, playlists);
}

std::size_t fill_short_ids(CacheHead* head) {
	std::vector<std::string> id_hashes;
	CacheNode* node = head->next;
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
		node->playlist.short_id = std::string(node->id_hash.data(), sid_len);
		node = node->next;
	}
	return sid_len;
}

PlaylistCache::const_iterator PlaylistCache::cbefore_begin() const { return const_iterator(head); }
PlaylistCache::const_iterator PlaylistCache::cbegin() const { return ++const_iterator(head); }
PlaylistCache::const_iterator PlaylistCache::cend() const { return const_iterator(); }
PlaylistCache::iterator PlaylistCache::before_begin() { return iterator(head); }
PlaylistCache::iterator PlaylistCache::begin() { return ++iterator(head); }
PlaylistCache::iterator PlaylistCache::end() { return iterator(); }
