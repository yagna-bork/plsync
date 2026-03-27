#include "../include/playlist_cache.h"
#include "../include/models.h"
#include "../include/config.h"
#include "../include/playlist_cache.pb.h"
#include "../include/util.h"
#include "../include/playlist_items_cache.h"
#include <cassert>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <ios>
#include <fstream>
#include <deque>
#include <unordered_map>

namespace fs = std::filesystem;

// TODO reduce number of tiles by bringing
// playlist_items_cache and sid_to_id_map files
// into this single file
namespace PlaylistCache {

void update_playlist(Playlist& pl, const proto::Playlist& proto_pl) {
	pl.id = std::string(proto_pl.id());
	pl.etag = std::string(proto_pl.etag());
	pl.version = std::string(proto_pl.version());
	pl.title = std::string(proto_pl.title());
	pl.is_private = proto_pl.is_private();
	pl.items = proto_pl.items();
}

Node::Node(const proto::CacheNode& proto_node)
	: id_hash(proto_node.id_hash()), items_id(proto_node.items_id()), was_changed(false)
{
	update_playlist(playlist, proto_node.playlist());
}

inline fs::path dir(Platform plat) { 
	fs::path cache_dir = get_setting("cache_dir");
	return cache_dir / "playlist" / platform_title_lower(plat); 
}

inline fs::path head_path(Platform plat) { return dir(plat) / "HEAD"; }

inline fs::path node_path(const Node* node, Platform plat) {
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
	if (it.ptr.is_head) {
		it.ptr.head->next = next;
		it.ptr.head->was_changed = true;
	} else {
		it.ptr.node->next = next;
		it.ptr.node->was_changed = true;
	}
}

void set_prev(Node* node, Head* head) {
	node->prev.is_head = true;
	node->prev.head = head;
}

void set_prev(Node* node, Node* prev) {
	node->prev.is_head = false;
	node->prev.node = prev;
}

inline void free_and_advance_node(Node** ptr) {
	Node* tmp = *ptr;
	*ptr = (*ptr)->next;
	delete tmp;
}

proto::CacheNode create_proto_node(const Node* node) {
	proto::CacheNode proto_node;
	proto_node.set_items_id(node->items_id);
	proto_node.set_id_hash(node->id_hash);

	auto proto_pl = proto_node.mutable_playlist();
	proto_pl->set_id(node->playlist.id);
	proto_pl->set_etag(node->playlist.etag);
	proto_pl->set_version(node->playlist.version);
	proto_pl->set_title(node->playlist.title);
	proto_pl->set_is_private(node->playlist.is_private);
	proto_pl->set_items(node->playlist.items);
	return proto_node;
}

const_iterator cbefore_begin(Head* head) { return const_iterator(head); }
const_iterator cbegin(Head* head) { return ++const_iterator(head); }
const_iterator cend() { return const_iterator(); }
iterator before_begin(Head* head) { return iterator(head); }
iterator begin(Head* head) { return ++iterator(head); }
iterator end() { return iterator(); }

Head* load(Platform plat) { 
	proto::CacheHead proto_head;
	{
		auto f = ensure_bin_file<std::ifstream>(head_path(plat));
		proto_head.ParseFromIstream(&f);
	}
	Head* head = new Head;
	head->etag = proto_head.etag();
	head->sid_len = proto_head.sid_len();
	std::string next_id(proto_head.next());
	if (next_id.empty()) {
		return head;
	}

	auto dummy_head = new Node;
	auto node = dummy_head;
	while (!next_id.empty()) {
		proto::CacheNode proto_node;
		std::ifstream f(dir(plat) / next_id, std::ios::binary);
		proto_node.ParseFromIstream(&f);

		auto new_node = new Node(proto_node);
		node->next = new_node;
		set_prev(new_node, node);
		next_id = std::string(proto_node.next());
		node = new_node;
	}
	head->next = dummy_head->next;
	delete dummy_head;
	set_prev(head->next, head);
	return head;
}

void save(Head* head, Platform plat) {
	// save and delete head
	if (head->was_changed) {
		proto::CacheHead proto_head;
		set_next(proto_head, head->next);
		proto_head.set_etag(head->etag);
		proto_head.set_sid_len(head->sid_len);
		std::ofstream f(head_path(plat), std::ios::binary);
		proto_head.SerializeToOstream(&f);
	}
	Node* node = head->next;
	delete head;

	// save nodes
	std::string prev_id = "HEAD";
	while (node) {
		if (!node->was_changed) {
			free_and_advance_node(&node);
			continue;
		}
		auto proto_node = create_proto_node(node);
		set_next(proto_node, node->next);
		proto_node.set_prev(prev_id);
		std::ofstream f(node_path(node, plat), std::ios::binary);
		proto_node.SerializeToOstream(&f);
		
		prev_id = node->playlist.id;
		free_and_advance_node(&node);
	}
}

void update(Head* head, Platform plat, const std::vector<Playlist>& playlists, const std::string& etag) {
	std::size_t n = playlists.size();
	std::deque<bool> is_new(n, true);
	std::unordered_map<std::string, std::size_t> version_to_idx;
	std::unordered_map<std::string, std::size_t> id_to_idx;
	for (std::size_t i = 0; i != n; i++) {
		version_to_idx[playlists[i].version] = i;
		id_to_idx[playlists[i].id] = i;
	}

	head->etag = etag;
	head->was_changed = true;

	auto prev = cbefore_begin(head);
	auto curr = cbegin(head);
	auto curr_write = begin(head);
	while (curr != cend()) {
		// case 1: version unchanged, playlist unchanged
		std::string version(curr->version);
		if (version_to_idx.count(version)) {
			std::size_t i = version_to_idx[version];
			is_new[i] = false;
			++prev; ++curr; ++curr_write;
			continue;
		}

		// case 2: version changed but id found, playlist changed
		std::string id(curr->id);
		if (id_to_idx.count(id)) {
			std::size_t i = id_to_idx[id];
			is_new[i] = false;
			*curr_write = playlists[i];
			++prev; ++curr; ++curr_write;
			continue;
		}
		
		// case 3: id not found, playlist deleted
		Node* tmp = curr.ptr.node;
		++curr; ++curr_write;
		set_next(prev, tmp->next);
		if (tmp->next) {
			tmp->next->prev = tmp->prev;
			tmp->next->was_changed = true;
		}
		fs::remove(node_path(tmp, plat));
		delete tmp;
	}

	// case 4: playlist not found in cache, new playlist
	curr = prev;
	for (std::size_t i = 0; i != n; i++) {
		if (!is_new[i]) {
			continue;
		}

		Node* new_node = new Node(playlists[i]);
		if (curr.ptr.is_head) {
			set_prev(new_node, curr.ptr.head);
		} else {
			set_prev(new_node, curr.ptr.node);
		}
		set_next(curr, new_node);
		++curr;
	}
}

std::size_t calculate_short_id_len(Head* head) {
	std::vector<std::string> id_hashes;
	Node* node = head->next;
	while (node) {
		id_hashes.emplace_back(node->id_hash);
		node = node->next;
	}

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
	return sid_len;
}

void fill_short_ids(Head* head, std::size_t sid_len) {
	if (sid_len != head->sid_len) {
		head->sid_len = sid_len;
		head->was_changed = true;
	}

	Node* node = head->next;
	while (node) {
		std::string short_id(node->id_hash.data(), sid_len);
		node->playlist.short_id = short_id;
		node = node->next;
	}
}

Node load_node(const std::string& id, Platform plat) {
	proto::CacheNode proto_node;
	{
		std::ifstream f(dir(plat) / id, std::ios::binary);
		proto_node.ParseFromIstream(&f);
	}
	Node node(proto_node);
	update_playlist(node.playlist, proto_node.playlist());
	return node;
}

void save_node(const Node& node, Platform plat) {
	proto::CacheNode tmp;
	auto proto_node = create_proto_node(&node);
	std::fstream f(dir(plat) / node.playlist.id, std::ios::binary|std::ios::out|std::ios::in);
	tmp.ParseFromIstream(&f);
	proto_node.set_next(tmp.next());
	proto_node.set_prev(tmp.prev());
	f.clear();
	f.seekp(0);
	proto_node.SerializeToOstream(&f);
}

void remove_node(Node& node, Platform plat) {
	proto::CacheNode tmp;
	{
		std::ifstream file(dir(plat) / node.playlist.id);
		tmp.ParseFromIstream(&file);
	}
	std::string prev(tmp.prev());
	std::string next(tmp.next());
	fs::remove(dir(plat) / node.playlist.id);
	
	if (prev == "HEAD") {
		proto::CacheHead head;
		std::fstream file(dir(plat) / prev);
		head.ParseFromIstream(&file);
		head.set_next(next);
		file.clear();
		file.seekp(0);
		head.SerializeToOstream(&file);
	} else {
		std::fstream file(dir(plat) / prev);
		tmp.ParseFromIstream(&file);
		tmp.set_next(next);
		file.clear();
		file.seekp(0);
		tmp.SerializeToOstream(&file);
	}
	
	if (!next.empty()) {
		std::fstream file(dir(plat) / next);
		tmp.ParseFromIstream(&file);
		tmp.set_prev(prev);
		file.clear();
		file.seekp(0);
		tmp.SerializeToOstream(&file);
	}
	node = Node();
}

void create_node(const Node& node, Platform plat) {
	proto::CacheHead head;
	std::string next;
	{
		auto file = ensure_bin_file<std::fstream>(head_path(plat), std::ios::in | std::ios::out);
		head.ParseFromIstream(&file);
		next = std::string(head.next());
		head.set_next(node.playlist.id);
		head.SerializeToOstream(&file);
	}

	auto proto_node = create_proto_node(&node);
	proto_node.set_next(next);
	proto_node.set_prev("HEAD");
	{
		std::ofstream file(node_path(&node, plat), std::ios::binary);
		proto_node.SerializeToOstream(&file);
	}
	
	if (next.empty()) {
		return;
	}
	std::fstream file(dir(plat) / next, std::ios::binary|std::ios::in|std::ios::out);
	proto_node.ParseFromIstream(&file);
	proto_node.set_prev(node.playlist.id);
	proto_node.SerializeToOstream(&file);
}

bool load_head(Platform plat, Head& res) {
	if (!fs::exists(head_path(plat))) return false;
	proto::CacheHead head;
	std::ifstream file(head_path(plat), std::ios::binary);
	head.ParseFromIstream(&file);
	res.etag = std::string(head.etag());
	res.sid_len = head.sid_len();
	return true;
}

const_iterator Handle::cbefore_begin() { return PlaylistCache::cbefore_begin(head); }
const_iterator Handle::cbegin() { return PlaylistCache::cbegin(head); }
const_iterator Handle::cend() { return PlaylistCache::cend(); }
iterator Handle::before_begin() { return PlaylistCache::before_begin(head); }
iterator Handle::begin() { return PlaylistCache::begin(head); }
iterator Handle::end() { return PlaylistCache::end(); }

}
