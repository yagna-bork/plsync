#include "../include/cache.h"
#include "../include/models.h"
#include "../include/config.h"
#include "../include/cache.pb.h"
#include "../include/util.h"
#include <deque>
#include <ios>
#include <filesystem>
#include <iostream> // TODO remove

MetaCache::MetaCache(Platform platform, const std::string &name) 
	: Cache(platform), subdir(parent_dir/name), head(parent_dir/name/"HEAD.pb") 
{
	std::filesystem::create_directory(subdir);
	if (std::filesystem::exists(head)) {
		return;
	}

	// set entry.id to 'HEAD' for head so save_node() and read_node()
	// can correctly identify its location by way of get_file_name()
	// i.e. subdir/HEAD.pb
	MetaCacheNode head_node;
	head_node.mutable_entry()->set_id("HEAD");
	save_node(head_node);
}

MetaCacheNode MetaCache::read_node(std::filesystem::path p) {
	MetaCacheNode node;
	std::ifstream f(p, std::ios::binary);
	node.ParseFromIstream(&f);
	return node;
}

/* Saves the contents of node into the correct file */
void MetaCache::save_node(const MetaCacheNode &node) {
	std::ofstream f(subdir / get_file_name(node), std::ios::binary);
	node.SerializeToOstream(&f);
}

void MetaCache::set_entry(MetaCacheEntry *entry, const Playlist &pl) {
	entry->set_id(pl.id);
	entry->set_etag(pl.etag);
	entry->set_title(pl.title);
	entry->set_is_private(pl.is_private);
	entry->set_items(pl.items);

	std::string id_hash;
	sha256(pl.id, id_hash);
	entry->set_id_hash(id_hash);
}

void MetaCache::update(const std::vector<Playlist> &playlists) {
	std::size_t n = playlists.size();
	std::deque<bool> is_new(n, true); // instead of std::vector<bool>
	MetaCacheNode node = read_node(head);

	MetaCacheNode new_node;
	for (std::size_t i = 0; i != n; i++) {
		if (!is_new[i]) {
			continue;
		}
		new_node.Clear();
		set_entry(new_node.mutable_entry(), playlists[i]);
		set_prev(new_node, node);
		save_node(new_node);
	
		set_next(node, new_node);
		save_node(node);
		node.CopyFrom(new_node);
	}
}
