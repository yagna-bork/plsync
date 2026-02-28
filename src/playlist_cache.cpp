#include "../include/cache.h"
#include "../include/util.h"
#include <cassert>
#include <deque>
#include <ios>
#include <filesystem>
#include <unordered_map>
#include <utility>
#include <memory>
#include "absl/strings/str_cat.h"

PlaylistCache::PlaylistCache(Platform platform, const std::string &name) 
	: Cache(platform), cache_dir(parent_dir/name), head_path(parent_dir/name/"HEAD.pb") 
{
	std::filesystem::create_directory(cache_dir);
	PlaylistCacheHead head;
	if (!std::filesystem::exists(head_path)) {
		// empty list is always sorted
		head.set_is_sorted(true);
		std::ofstream file(head_path, std::ios::binary);
		head.SerializeToOstream(&file);
	} else {
		std::ifstream file(head_path, std::ios::binary);
		head.ParseFromIstream(&file);
	}
	is_sorted = head.is_sorted();
	return;
}

PlaylistCache::~PlaylistCache() {
	PlaylistCacheHead head;
	head.set_is_sorted(is_sorted);
	std::ofstream file(head_path, std::ios::binary);
	head.SerializeToOstream(&file);
}


/* forward_list interface BEGIN */
PlaylistCache::const_iterator PlaylistCache::cbefore_begin() {
	return const_iterator(cache_dir, Position::HEAD, head_path.filename());
}

PlaylistCache::const_iterator PlaylistCache::cbegin() {
	return ++cbefore_begin();
}

PlaylistCache::const_iterator PlaylistCache::cend() {
	return const_iterator(cache_dir, Position::END);
}

PlaylistCache::iterator PlaylistCache::before_begin() {
	return iterator(cache_dir, Position::HEAD, head_path.filename());
}

PlaylistCache::iterator PlaylistCache::begin() {
	return ++before_begin();
}

PlaylistCache::iterator PlaylistCache::end() {
	return iterator(cache_dir, Position::END);
}

template <bool is_const>
PlaylistCache::Iterator<is_const> PlaylistCache::insert_after(Iterator<is_const>& pos, const Playlist& playlist) {
	assert(pos != end());
	PlaylistCacheNode node;
	node.set_next(pos.next());
	*node.mutable_entry() = get_entry(playlist);

	std::string fname = playlist.id + ".pb";
	{
		std::ofstream file(cache_dir / fname, std::ios::binary);
		node.SerializeToOstream(&file);
	}
	pos.set_next(fname);
	return iterator(cache_dir, Position::BETWEEN, fname);
}

template<bool is_const>
PlaylistCache::Iterator<is_const> PlaylistCache::erase_after(Iterator<is_const>& pos) {
	assert(pos != end());
	std::string next = pos.next();
	if (next.empty()) {
		return end();
	}
	const_iterator tmp = pos;
	++tmp;
	std::string next_next = tmp.next();

	std::filesystem::remove(cache_dir / next);
	pos.set_next(next_next);
	return next_next.empty() ? end() : iterator(cache_dir, Position::BETWEEN, next_next);
}

template <bool is_const>
void PlaylistCache::update_at(Iterator<is_const>& pos, const Playlist& playlist) {
	assert(pos != before_begin() && pos != end());
	PlaylistCacheNode node;
	node.set_next(pos.next());
	*node.mutable_entry() = get_entry(playlist);
	std::string name(absl::StrCat(pos->id(), ".pb"));
	{
		std::ofstream file(cache_dir / name, std::ios::binary);	
		node.SerializeToOstream(&file);
	}

	// pos has been invalidated so refresh it
	pos = iterator(cache_dir, Position::BETWEEN, name);
}


PlaylistCacheEntry PlaylistCache::get_entry(const Playlist& playlist) {
	PlaylistCacheEntry entry;
	entry.set_id(playlist.id);
	entry.set_etag(playlist.etag);
	entry.set_title(playlist.title);
	entry.set_is_private(playlist.is_private);
	entry.set_items(playlist.items);

	std::string id_hash;
	sha256(playlist.id, id_hash);
	entry.set_id_hash(id_hash);
	return entry;
}

void PlaylistCache::read_node_from_path(std::filesystem::path p, PlaylistCacheNode &node) {
	if (!std::filesystem::exists(p)) {
		node.Clear();
		return;
	}
	std::ifstream f(p, std::ios::binary);
	node.ParseFromIstream(&f);
}

/* Saves the contents of node into the correct file */
void PlaylistCache::save_node(const PlaylistCacheNode &node) {
	std::ofstream f(cache_dir / get_file_name(node), std::ios::binary);
	node.SerializeToOstream(&f);
}

void PlaylistCache::set_entry(PlaylistCacheEntry *entry, const Playlist &pl, bool set_id_hash) {
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


/* External interface */
void PlaylistCache::update(const std::vector<Playlist> &playlists) {
	std::size_t n = playlists.size();
	std::deque<bool> is_new(n, true); // instead of std::vector<bool>
	std::unordered_map<std::string, std::size_t> etag_to_idx;
	std::unordered_map<std::string, std::size_t> id_to_idx;
	for (std::size_t i = 0; i != n; i++) {
		etag_to_idx[playlists[i].etag] = i;
		id_to_idx[playlists[i].id] = i;
	}

	auto prev = cbefore_begin();
	auto curr = cbegin();
	while (curr != cend()) {
		// case 1: same etag so playlist unchanged
		std::string etag(curr->etag());
		if (etag_to_idx.count(etag)) {
			std::size_t idx = etag_to_idx[etag];
			is_new[idx] = false;
			prev = curr++;
			continue;
		}

		// case 2: etag changed so update cache
		std::string id(curr->id());
		if (id_to_idx.count(id)) {
			std::size_t idx = id_to_idx[id];
			is_new[idx] = false;
			if (curr->title() != playlists[idx].title) {
				is_sorted = false;
			}
			update_at(curr, playlists[idx]);
			prev = curr++;
			continue;
		}

		// case 3: playlist id not found, delete from cache
		curr = erase_after(prev);
		is_sorted = false;
	}

	// case 4: all remaining playlists must be new
	curr = std::move(prev);
	for (std::size_t i = 0; i != n; i++) {
		if (!is_new[i]) {
			continue;
		}
		curr = insert_after(curr, playlists[i]);
		is_sorted = false;
	}
}

std::vector<Playlist> PlaylistCache::get_playlists() {
	std::vector<Playlist> playlists;
	std::vector<std::string> id_hashes;
	PlaylistCacheNode node;
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

std::vector<Playlist> PlaylistCache::get_playlists_sorted() {
	if (is_sorted) {
		return get_playlists();
	}

	PlaylistCacheNode node;
	read_node_from_path(head_path, node);
	read_node(node.next(), node);
	std::vector<std::pair<std::string, std::string>> titles_nodes;
	while (node.has_entry()) {
		titles_nodes.emplace_back(node.entry().title(), get_file_name(node));
		read_node(node.next(), node);
	}
	std::sort(titles_nodes.begin(), titles_nodes.end());

	PlaylistCacheNode prev, curr;
	read_node_from_path(head_path, prev);
	for (const auto &title_node: titles_nodes)  {
		read_node(title_node.second, curr);
		set_next(prev, curr);
		curr.clear_next();
		save_node(prev);
		save_node(curr);
		prev.CopyFrom(curr);
	}
	is_sorted = true;
	return get_playlists();
}


/* Iterator */
template <bool is_const>
PlaylistCache::Iterator<is_const>::Iterator(
	const std::filesystem::path &cache_dir, Position pos, const std::string &name
) : cache_dir(cache_dir), pos(pos), was_changed(false), node(std::make_shared<PlaylistCacheNode>()),
	file()
{
	if (pos == Position::END) return;
	file = std::make_shared<std::fstream>(
		cache_dir / name, std::ios::in | std::ios::out | std::ios::binary
	);
	node->ParseFromIstream(file.get());
}


/* RULE OF 5 */
template <bool is_const> 
template <bool is_other_const>
PlaylistCache::Iterator<is_const>::Iterator(const Iterator<is_other_const> &other)
	: cache_dir(other.cache_dir), pos(other.pos), was_changed(false), file(other.file), node(other.node)
{
	if (!is_const) {
		if (is_other_const) { 
			throw std::domain_error("Attempted to upgrade const iterator to non-const");
		} else {
			throw std::domain_error("Attempted to duplicate non-const iterator");
		}
	}
	other.save();
}

template <bool is_const>
template <bool is_rhs_const>
PlaylistCache::Iterator<is_const>& PlaylistCache::Iterator<is_const>::operator=(const Iterator<is_rhs_const> &rhs) {
	if (!is_const) {
		if (is_rhs_const) { 
			throw std::domain_error("Attempted to upgrade const iterator to non-const");
		} else {
			throw std::domain_error("Attempted to duplicate non-const iterator");
		}
	}
	save();
	rhs.save();
	file = rhs.file;
	node = rhs.node;
	pos = rhs.pos;
	return *this;
}

template <bool is_const>
template <bool is_other_const>
PlaylistCache::Iterator<is_const>::Iterator(Iterator<is_other_const> &&other)
	: cache_dir(std::move(other.cache_dir)), file(std::move(other.file)), node(std::move(other.node)),
	  pos(other.pos)
{
	if (!is_const && is_other_const) {
		throw std::domain_error("Attempted to upgrade const iterator to non-const");
	}
	if (is_const == is_other_const) {
		was_changed = other.was_changed;
		other.was_changed = false; // prevent destructor from saving cleared/moved node
	} else {
		other.save();
		was_changed = false;
	}
}

template <bool is_const>
template <bool is_rhs_const>
PlaylistCache::Iterator<is_const>& PlaylistCache::Iterator<is_const>::operator=(Iterator<is_rhs_const> &&rhs) {
	if (!is_const && is_rhs_const) {
		throw std::domain_error("Attempted to upgrade const iterator to non-const");
	}

	if (is_const && !is_rhs_const) {
		// const iterator can't save changes to PlaylistCacheEntry 
		// so make non-const iterator save them first
		rhs.save();
	}

	save();
	file = std::move(rhs.file);
	node = std::move(rhs.node);
	pos = rhs.pos;
	was_changed = rhs.was_changed;

	if (is_const == is_rhs_const) {
		rhs.was_changed = false; // prevent destructor from saving cleared/moved node
	}
	return *this;
}


/* Iterator interface */
template <bool is_const>
typename PlaylistCache::Iterator<is_const>::reference PlaylistCache::Iterator<is_const>::operator*() {
	// assume change made when non-const iterator dereferenced
	was_changed = !is_const; 
	return node->entry(); 
}

template <bool is_const>
typename PlaylistCache::Iterator<is_const>::pointer PlaylistCache::Iterator<is_const>::operator->() { 
	// assume change made when non-const iterator dereferenced
	was_changed = !is_const;
	return &node->entry(); 
}

template <bool is_const>
bool PlaylistCache::Iterator<is_const>::operator==(const Iterator<is_const> &rhs) const { 
	if (pos != Position::BETWEEN) return pos == rhs.pos;
	return node->entry().id() == rhs.node->entry().id();
}

template <bool is_const>
inline bool PlaylistCache::Iterator<is_const>::operator!=(const Iterator<is_const> &rhs) const { 
	return !(*this == rhs); 
}

template <bool is_const>
PlaylistCache::Iterator<is_const>& PlaylistCache::Iterator<is_const>::operator++() {
	if (pos == Position::END) return *this; // undefined
	save();
	if (node->next().empty()) {
		file.reset();
		node->Clear();
		pos = Position::END;
		return *this;
	}
	file = std::make_shared<std::fstream>(cache_dir/node->next(), std::ios::in | std::ios::out | std::ios::binary);
	node->ParseFromIstream(file.get());
	pos = Position::BETWEEN;
	return *this;
}

template <bool is_const>
PlaylistCache::Iterator<is_const> PlaylistCache::Iterator<is_const>::operator++(int) {
	auto tmp = *this;
	++(*this);
	return tmp;
}


/* Other */
template <bool is_const>
void PlaylistCache::Iterator<is_const>::save() {
	if (!was_changed || pos == Position::END) return;
	file->clear();
	file->seekp(0);
	node->SerializeToOstream(file.get());
	was_changed = false;
}

template <bool is_const>
std::string PlaylistCache::Iterator<is_const>::next() const {
	return std::string(node->next());
}

template <bool is_const>
void PlaylistCache::Iterator<is_const>::set_next(std::string fname) {
	node->set_next(fname);
	was_changed = true;
}
