#include "../include/cache.h"
#include "../include/models.h"
#include "../include/config.h"
#include "../include/cache.pb.h"
#include "../include/util.h"
#include <deque>
#include <ios>
#include <filesystem>
#include <unordered_map>
#include <utility>

MetaCache::MetaCache(Platform platform, const std::string &name) 
	: Cache(platform), subdir(parent_dir/name), head_path(parent_dir/name/"HEAD.pb") 
{
	std::filesystem::create_directory(subdir);
	if (std::filesystem::exists(head_path)) {
		return;
	}

	// set entry.id to 'HEAD' for head so save_node() and read_node()
	// can correctly identify its location by way of get_file_name()
	// i.e. subdir/HEAD.pb
	MetaCacheNode head;
	head.mutable_entry()->set_id("HEAD");
	save_node(head);
	// empty list is always sorted
	set_is_sorted(true);
}

void MetaCache::read_node_from_path(std::filesystem::path p, MetaCacheNode &node) {
	if (!std::filesystem::exists(p)) {
		node.Clear();
		return;
	}
	std::ifstream f(p, std::ios::binary);
	node.ParseFromIstream(&f);
}

/* Saves the contents of node into the correct file */
void MetaCache::save_node(const MetaCacheNode &node) {
	std::ofstream f(subdir / get_file_name(node), std::ios::binary);
	node.SerializeToOstream(&f);
}

void MetaCache::set_entry(MetaCacheEntry *entry, const Playlist &pl, bool set_id_hash) {
	entry->set_id(pl.id);
	entry->set_etag(pl.etag);
	entry->set_title(pl.title);
	entry->set_is_private(pl.is_private);
	entry->set_items(pl.items);

	if (!set_id_hash) {
		return;
	}
	std::string id_hash;
	sha256(pl.id, id_hash);
	entry->set_id_hash(id_hash);
}

void MetaCache::set_is_sorted(bool val) {
	MetaCacheHead head;
	std::fstream f(head_path, std::ios::binary);
	head.ParseFromIstream(&f);
	head.set_is_sorted(val);
	head.SerializeToOstream(&f);
}

bool MetaCache::is_sorted() {
	MetaCacheHead head;
	std::ifstream f(head_path, std::ios::binary);
	head.ParseFromIstream(&f);
	return head.is_sorted();
}

void MetaCache::update(const std::vector<Playlist> &playlists) {
	std::size_t n = playlists.size();
	std::deque<bool> is_new(n, true); // instead of std::vector<bool>
	std::unordered_map<std::string, std::size_t> etag_to_idx;
	std::unordered_map<std::string, std::size_t> id_to_idx;
	for (std::size_t i = 0; i != n; i++) {
		etag_to_idx[playlists[i].etag] = i;
		id_to_idx[playlists[i].id] = i;
	}

	bool order_changed = false;
	MetaCacheNode prev, curr;
	read_node_from_path(head_path, prev);
	read_node(prev.next(), curr);
	while (curr.has_entry()) {
		// case 1: same etag so playlist unchanged
		std::string etag(curr.entry().etag());
		if (etag_to_idx.count(etag)) {
			std::size_t idx = etag_to_idx[etag];
			is_new[idx] = false;
			prev.CopyFrom(curr);
			read_node(curr.next(), curr);
			continue;
		}
		
		// case 2: diff etag but playlist id 
		// still exists so must update cache
		std::string id(curr.entry().id());
		if (id_to_idx.count(id)) {
			std::size_t idx = id_to_idx[id];
			is_new[idx] = false;
			bool title_changed = id != playlists[idx].id;
			set_entry(
				curr.mutable_entry(), playlists[idx], /*set_id_hash=*/title_changed
			);
			save_node(curr);
			if (title_changed) {
				order_changed = true;
			}

			prev.CopyFrom(curr);
			read_node(curr.next(), curr);
			continue;
		}
		
		// case 3: playlist id not found
		// delete from cache
		prev.set_next(curr.next());
		save_node(prev);
		order_changed = true;
		std::filesystem::remove(subdir / get_file_name(curr));
		read_node(curr.next(), curr);
	}

	/* 
	 * case 4: new playlists.
	 * deal with the remaining by appending them to end of cache.
	 * loop invariant: prev points to last node in cache and curr 
	 * is one past the end meaning it's cleared and free for writes
	 */
	for (std::size_t i = 0; i != n; i++) {
		if (!is_new[i]) {
			continue;
		}
		set_entry(curr.mutable_entry(), playlists[i]);
		set_prev(curr, prev);
		save_node(curr);
		set_next(prev, curr);
		save_node(prev);
		order_changed = true;

		// maintain invariant
		prev.CopyFrom(curr);
		read_node(curr.next(), curr);
	}

	// empty cache is still sorted
	if (playlists.empty()) {
		set_is_sorted(true);
	} else if (order_changed) { 
		set_is_sorted(false);
	}
}

std::vector<Playlist> MetaCache::get_playlists() {
	std::vector<Playlist> playlists;
	std::vector<std::string> id_hashes;
	MetaCacheNode node;
	read_node_from_path(head_path, node);
	read_node(node.next(), node);

	while (node.has_entry()) {
		playlists.emplace_back(
			std::string(node.entry().id()),
			std::string(node.entry().etag()),
			std::string(node.entry().title()),
			node.entry().is_private(),
			node.entry().items()
		);
		id_hashes.push_back(std::string(node.entry().id_hash()));
		read_node(node.next(), node);
	}

	/* determine min length of short_id (sid) to make all unique */
	std::size_t n = playlists.size();
	std::vector<std::size_t> collision_idxs(n);
	std::iota(collision_idxs.begin(), collision_idxs.end(), 0);

	std::unordered_map<std::string, std::vector<std::size_t>> sid_groups;
	std::size_t sid_len = 0;
	while (!collision_idxs.empty()) {
		sid_len++;
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
		sid_groups.clear();
	}
	
	/* paste in short_ids of calculated length */
	for (std::size_t i = 0; i != n; i++) {
		playlists[i].short_id = std::string(id_hashes[i].data(), sid_len);
	}
	return playlists;
}

std::vector<Playlist> MetaCache::get_playlists_sorted() {
	if (is_sorted()) {
		return get_playlists();
	}

	MetaCacheNode node;
	read_node_from_path(head_path, node);
	read_node(node.next(), node);
	std::vector<std::pair<std::string, std::string>> titles_nodes;
	while (node.has_entry()) {
		titles_nodes.emplace_back(node.entry().title(), get_file_name(node));
		read_node(node.next(), node);
	}
	std::sort(titles_nodes.begin(), titles_nodes.end());

	MetaCacheNode prev, curr;
	read_node_from_path(head_path, prev);
	for (const auto &title_node: titles_nodes)  {
		read_node(title_node.second, curr);
		set_next(prev, curr);
		set_prev(curr, prev);
		curr.clear_next();
		save_node(prev);
		save_node(curr);
		prev.CopyFrom(curr);
	}
	set_is_sorted(true);
	return get_playlists();
}
