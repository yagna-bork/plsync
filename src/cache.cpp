#include "../include/cache.h"
#include "../include/cache.pb.h"
#include "../include/util.h"
#include <algorithm>
#include <cassert>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <google/protobuf/repeated_field.h>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

static Song _get_song(const proto::Song& proto_song) {
    Song song;
    for (const auto& artist : proto_song.artists()) {
        song.artists.emplace_back(artist);
    }
    song.track = std::string(proto_song.track());
    return song;
}

static proto::Song _get_proto_song(const Song& song) {
    proto::Song proto_song;
    for (const std::string& artist : song.artists) {
        proto_song.add_artists(artist);
    }
    proto_song.set_track(song.track);
    return proto_song;
}

static proto::Platform _get_proto_platform(Platform plat) {
    switch (plat) {
    case Platform::YOUTUBE:
        return proto::Platform::YOUTUBE;
    case Platform::SPOTIFY:
        return proto::Platform::SPOTIFY;
#ifndef NDEBUG
    case Platform::TEST:
        return proto::Platform::TEST;
#endif
    default:
        return proto::Platform::INVALID;
    }
}

static Platform _get_platform(proto::Platform plat) {
    switch (plat) {
    case proto::Platform::YOUTUBE:
        return Platform::YOUTUBE;
    case proto::Platform::SPOTIFY:
        return Platform::SPOTIFY;
#ifndef NDEBUG
    case proto::Platform::TEST:
        return Platform::TEST;
#endif // !NDEBUG
    default:
        return Platform::INVALID;
    }
}

bool Song::operator==(const Song& rhs) const {
    return artists == rhs.artists && track == rhs.track;
}

std::ostream& Song::operator<<(std::ostream& os) {
    os << track << " - ";
    for (int i = 0; i != artists.size(); i++) {
        if (i != 0) {
            os << ",";
        }
        os << artists[i];
    }
    return os;
}

size_t std::hash<Song>::operator()(const Song& song) const {
    // https://stackoverflow.com/a/27216842
    // https://stackoverflow.com/a/20602159
    size_t seed = song.artists.size();
    std::hash<std::string> hasher;
    for (const std::string& artist : song.artists) {
        seed ^= hasher(artist) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed ^ hasher(song.track);
}

void Playlist::merge(Playlist&& other) {
    std::string tmp;
    if (!other.id.empty()) {
        id = std::move(other.id);
    } else {
        tmp = std::move(other.id);
    }

    if (!other.id_hash.empty()) {
        id_hash = std::move(other.id_hash);
    } else {
        tmp = std::move(other.id_hash);
    }

    if (!other.etag.empty()) {
        etag = std::move(other.etag);
    } else {
        tmp = std::move(other.etag);
    }

    if (!other.version.empty()) {
        version = std::move(other.version);
    } else {
        tmp = std::move(other.version);
    }

    if (!other.title.empty()) {
        title = std::move(other.title);
    } else {
        tmp = std::move(other.title);
    }

    if (other.plat != Platform::INVALID) {
        plat = std::move(other.plat);
    } else {
        Platform plat = std::move(other.plat);
    }

    if (!other.tracker.empty()) {
        tracker = std::move(other.tracker);
    } else {
        tmp = std::move(other.tracker);
    }
    items.merge(std::move(other.items));
    is_private = (is_private == other.is_private) ? is_private : true;
    num_items = other.num_items != 0 ? other.num_items : num_items;
}

void PlaylistItems::merge(PlaylistItems&& other) {
    if (other.etag.empty()) {
        auto etag = std::move(other.etag);
        auto data = std::move(other.data);
    } else {
        was_changed = other.was_changed;
        etag = std::move(other.etag);
        data = std::move(other.data);
    }
}

Playlist::Playlist(const proto::Playlist& proto_pl)
    : id(std::string(proto_pl.id())), id_hash(std::string(proto_pl.id_hash())),
      title(std::string(proto_pl.title())),
      plat(_get_platform(proto_pl.plat())), is_private(proto_pl.is_private()),
      etag(std::string(proto_pl.etag())),
      version(std::string(proto_pl.version())), num_items(proto_pl.num_items()),
      tracker(proto_pl.tracker()) {}

Playlist::Playlist(std::string&& id, std::string&& etag, std::string&& version,
                   std::string&& title, Platform plat, bool is_private,
                   size_t num_items)
    : id(std::move(id)), etag(std::move(etag)), version(std::move(version)),
      title(std::move(title)), plat(plat), is_private(is_private),
      num_items(num_items) {
    id_hash = sha1(this->id);
}

Playlist Playlist::load_from_id_hash(const std::string& id_hash,
                                     Platform plat) {
    return _load(PlaylistTree(plat).search_id_hash(id_hash));
}

Playlist Playlist::load_from_sid(const std::string& sid, Platform plat) {
    return _load(PlaylistTree(plat).search_sid(sid));
}

void Playlist::add() {
    PlaylistTree pl_tree(plat);
    proto::CacheHead head;
    std::string next;
    {
        auto file = ensure_bin_file<std::fstream>(pl_tree.head(),
                                                  std::ios::in | std::ios::out);
        head.ParseFromIstream(&file);
        next = std::string(head.next());
        head.set_next(id_hash);
        head.SerializeToOstream(&file);
    }

    auto proto_node = _proto_node();
    proto_node.set_next(next);
    proto_node.set_prev("HEAD");
    {
        std::ofstream file(pl_tree.add(id_hash), std::ios::binary);
        proto_node.SerializeToOstream(&file);
    }

    if (next.empty()) {
        return;
    }
    std::fstream file(pl_tree.search_id_hash(next),
                      std::ios::binary | std::ios::in | std::ios::out);
    proto_node.ParseFromIstream(&file);
    proto_node.set_prev(id_hash);
    proto_node.SerializeToOstream(&file);
}

void Playlist::save() {
    PlaylistTree pl_tree(plat);
    if (!was_changed) {
        return;
    }
    proto::CacheNode node;
    std::fstream f(_path(), std::ios::binary | std::ios::out | std::ios::in);
    // we need `next` and `prev` from the old node
    node.ParseFromIstream(&f);
    node.MergeFrom(_proto_node());
    f.clear();
    f.seekp(0);
    node.SerializeToOstream(&f);
    was_changed = false;
}

void Playlist::save(const std::string& prev_id_hash,
                    const std::string& next_id_hash) {
    PlaylistTree pl_tree(plat);
    if (!was_changed) {
        return;
    }
    proto::CacheNode node = _proto_node();
    node.set_prev(prev_id_hash);
    node.set_next(next_id_hash);

    std::ofstream f(_path(), std::ios::binary);
    node.SerializeToOstream(&f);
    was_changed = false;
}

void Playlist::remove() {
    PlaylistTree pl_tree = PlaylistTree(plat);
    proto::CacheNode tmp;
    {
        std::ifstream file(pl_tree.search_id_hash(id_hash));
        tmp.ParseFromIstream(&file);
    }
    std::string prev(tmp.prev());
    std::string next(tmp.next());
    pl_tree.erase(id_hash);

    if (prev == "HEAD") {
        proto::CacheHead head;
        std::fstream file(pl_tree.head());
        head.ParseFromIstream(&file);
        head.set_next(next);
        file.clear();
        file.seekp(0);
        head.SerializeToOstream(&file);
    } else {
        std::fstream file(pl_tree.search_id_hash(prev));
        tmp.ParseFromIstream(&file);
        tmp.set_next(next);
        file.clear();
        file.seekp(0);
        tmp.SerializeToOstream(&file);
    }

    if (!next.empty()) {
        std::fstream file(pl_tree.search_id_hash(next));
        tmp.ParseFromIstream(&file);
        tmp.set_prev(prev);
        file.clear();
        file.seekp(0);
        tmp.SerializeToOstream(&file);
    }
    *this = Playlist();
}

Playlist Playlist::_load(const std::filesystem::path& path) {
    if (!fs::exists(path)) {
        return Playlist();
    }
    proto::CacheNode proto_node;
    std::ifstream f(path, std::ios::binary);
    proto_node.ParseFromIstream(&f);
    return Playlist(proto_node.playlist());
}

proto::CacheNode Playlist::_proto_node() {
    proto::CacheNode node;
    auto* proto_pl = node.mutable_playlist();
    proto_pl->set_id(id);
    proto_pl->set_id_hash(id_hash);
    proto_pl->set_title(title);
    proto_pl->set_plat(_get_proto_platform(plat));
    proto_pl->set_is_private(is_private);
    proto_pl->set_etag(etag);
    proto_pl->set_version(version);
    proto_pl->set_num_items(num_items);
    proto_pl->set_tracker(tracker);
    return node;
}

inline fs::path Playlist::_path() {
    PlaylistTree pl_tree(plat);
    fs::path p = pl_tree.search_id_hash(id_hash);
    if (p.empty()) {
        p = pl_tree.add(id_hash);
    }
    return p;
}

PlaylistTree::PlaylistTree(Platform plat) {
    assert(plat != Platform::INVALID);
    root = fs::path(get_setting("cache_dir")) / "playlist" /
           platform_title_lower(plat);
    fs::create_directories(root);
}

fs::path PlaylistTree::head() const { return root / "HEAD"; }

bool PlaylistTree::_is_leaf(const fs::directory_iterator& it) const {
    return (it != fs::end(it)) && (it->is_regular_file()) &&
           (it->path() != head());
}

void PlaylistTree::_search(const std::string& id_hash, fs::path dir, int depth,
                           fs::path& out_path, std::string& out_sid) const {
    auto it = fs::directory_iterator(dir);
    if (_is_leaf(it)) {
        if (it->path().filename() != bin_to_hex(id_hash)) {
            return;
        }
        out_path = it->path();
        out_sid = std::string(id_hash.begin(), id_hash.begin() + depth);
        return;
    } else {
        if (!fs::exists(dir / bin_to_hex(id_hash[depth], /*upper=*/false))) {
            return;
        }
        _search(id_hash, dir / bin_to_hex(id_hash[depth], /*upper=*/false),
                depth + 1, out_path, out_sid);
    }
}

fs::path PlaylistTree::search_id_hash(const std::string& id_hash) const {
    fs::path path;
    std::string _;
    _search(id_hash, root, 0, path, _);
    return path;
}

fs::path PlaylistTree::search_sid(const std::string& sid) const {
    fs::path dir = root;
    fs::directory_iterator it;
    for (char c : sid) {
        dir /= bin_to_hex(c, /*upper=*/false);
        if (!fs::exists(dir)) {
            return fs::path();
        }

        it = fs::directory_iterator(dir);
        if (!_is_leaf(it)) {
            continue;
        }

        std::string id_hash = hex_to_bin(it->path().filename());
        if (std::equal(sid.begin(), sid.end(), id_hash.begin())) {
            return it->path();
        }
        return fs::path();
    }
    return fs::path();
}

int PlaylistTree::_height(const fs::path& dir) const {
    int height = 0;
    auto it = fs::directory_iterator(dir);
    if (it == fs::end(it) || _is_leaf(it)) {
        return height;
    }
    while (it != fs::end(it)) {
        if (it->is_directory()) {
            height = std::max(height, _height(it->path()) + 1);
        }
        it++;
    }
    return height;
}

int PlaylistTree::height() const { return _height(root); }

fs::path PlaylistTree::_add(const fs::path& dir, const std::string& id_hash,
                            int depth) {
    auto it = fs::directory_iterator(dir);
    if (PlaylistTree::_is_leaf(it)) {
        fs::path leaf = it->path();
        std::string other_id_hash_hex = leaf.filename().string();
        std::string byte_hex(other_id_hash_hex.data() + 2 * depth, 2);
        fs::path subdir = dir / byte_hex;
        fs::create_directory(subdir);
        fs::rename(leaf, subdir / other_id_hash_hex);
        return _add(dir, id_hash, depth);
    }

    fs::path subdir = dir / bin_to_hex(id_hash[depth], /*upper=*/false);
    if (fs::exists(subdir)) {
        return _add(subdir, id_hash, depth + 1);
    }

    fs::path path = subdir / bin_to_hex(id_hash);
    ensure_bin_file<std::ofstream>(path);
    return path;
}

fs::path PlaylistTree::add(const std::string& id_hash) {
    return _add(root, id_hash, /*depth=*/0);
}

void PlaylistTree::_trim_path(const fs::path& dir) {
    if (dir == root) {
        return;
    }

    auto it = fs::directory_iterator(dir);
    if (++it != fs::end(it)) {
        // there is more than one subtree, path can't be trimmed further
        return;
    }

    it = fs::directory_iterator(dir);
    auto sub_it = fs::directory_iterator(it->path());
    if (!_is_leaf(sub_it)) {
        // the other subtree has nested subtrees, path can't be trimmed further
        return;
    }
    // trim path by moving leaf up one level into dir
    fs::rename(sub_it->path(), dir / sub_it->path().filename());
    fs::remove(it->path());
    _trim_path(dir.parent_path());
}

void PlaylistTree::erase(const std::string& id_hash) {
    fs::path leaf = search_id_hash(id_hash);
    assert(fs::exists(leaf));
    fs::remove(leaf);
    fs::remove(leaf.parent_path());
    _trim_path(leaf.parent_path().parent_path());
}

PlaylistCache::PlaylistCache(Platform plat)
    : plat(plat), pl_tree(plat), was_changed(false) {
    proto::CacheHead proto_head;
    {
        auto f = ensure_bin_file<std::ifstream>(pl_tree.head());
        proto_head.ParseFromIstream(&f);
    }
    etag = proto_head.etag();
    std::string next(proto_head.next());
    if (next.empty()) {
        return;
    }

    auto curr = playlists.before_begin();
    while (!next.empty()) {
        proto::CacheNode proto_node;
        fs::path p = pl_tree.search_id_hash(next);
        std::ifstream f(p, std::ios::binary);
        proto_node.ParseFromIstream(&f);
        curr = playlists.emplace_after(curr, proto_node.playlist());
        next = std::string(proto_node.next());
    }
}

void PlaylistCache::update(std::vector<Playlist>&& playlists,
                           const std::string& etag) {
    std::size_t n = playlists.size();
    std::deque<bool> is_new(n, true);
    std::unordered_map<std::string, std::size_t> version_to_idx;
    std::unordered_map<std::string, std::size_t> id_to_idx;
    for (std::size_t i = 0; i != n; i++) {
        version_to_idx[playlists[i].version] = i;
        id_to_idx[playlists[i].id] = i;
    }

    this->etag = etag;
    was_changed = true;
    auto prev = this->playlists.before_begin();
    auto curr = this->playlists.begin();
    while (curr != this->playlists.end()) {
        // case 1: version unchanged, playlist unchanged
        std::string version(curr->version);
        if (version_to_idx.count(version)) {
            std::size_t i = version_to_idx[version];
            is_new[i] = false;
            ++prev;
            ++curr;
            continue;
        }

        // case 2: version changed but id found, playlist changed
        std::string id(curr->id);
        if (id_to_idx.count(id)) {
            std::size_t i = id_to_idx[id];
            is_new[i] = false;
            curr->merge(std::move(playlists[i]));
            curr->was_changed = true;
            ++prev;
            ++curr;
            continue;
        }

        // case 3: id not found, playlist deleted
        curr->remove();
        curr = this->playlists.erase_after(prev);
        // force refresh of prev,next in proto::CacheNode
        prev->was_changed = true;
        if (curr != this->playlists.end()) {
            curr->was_changed = true;
        }
    }

    // case 4: playlist not found in cache, new playlist
    curr = prev;
    for (std::size_t i = 0; i != n; i++) {
        if (!is_new[i]) {
            continue;
        }
        // force refresh of next in proto::CacheNode or proto::CacheHead
        if (curr != this->playlists.before_begin()) {
            curr->was_changed = true;
        } else {
            was_changed = true;
        }
        curr = this->playlists.insert_after(curr, std::move(playlists[i]));
        curr->was_changed = true;
    }
}

void PlaylistCache::save() {
    auto curr = playlists.before_begin();
    if (was_changed) {
        proto::CacheHead proto_head;
        proto_head.set_next(_next_id_hash(curr));
        proto_head.set_etag(etag);
        std::ofstream f(pl_tree.head(), std::ios::binary);
        proto_head.SerializeToOstream(&f);
        was_changed = false;
    }

    // save nodes
    curr++;
    std::string prev_id_hash = "HEAD";
    while (curr != playlists.end()) {
        curr->save(prev_id_hash, _next_id_hash(curr));
        prev_id_hash = curr->id_hash;
        curr++;
    }
}

std::string
PlaylistCache::_next_id_hash(std::forward_list<Playlist>::const_iterator it) {
    auto next = it;
    next++;
    return (next != playlists.end()) ? next->id_hash : "";
}

inline fs::path PlaylistTracker::dir() {
    return fs::path(get_setting("cache_dir")) / "playlist_tracker";
}

inline fs::path PlaylistTracker::_playlist_items_dir(Platform plat) {
    return fs::path(get_setting("cache_dir")) / "playlist_items" /
           platform_title_lower(plat);
}

PlaylistTracker::PlaylistTracker(const std::string& id)
    : id(id), was_changed(false), path(dir() / id) {
    assert(fs::exists(path));
    proto::PlaylistTracker proto_tracker;
    {
        std::ifstream f(path, std::ios::binary);
        proto_tracker.ParseFromIstream(&f);
    }

    for (const auto p1 : proto_tracker.plat_id_hash_pairs()) {
        Playlist pl = Playlist::load_from_id_hash(std::string(p1.id_hash()),
                                                  _get_platform(p1.plat()));
        proto::PlaylistItems proto_items;
        {
            auto f = ensure_bin_file<std::ifstream>(
                _playlist_items_dir(pl.plat) / pl.id);
            proto_items.ParseFromIstream(&f);
        }

        pl.items.etag = std::string(proto_items.etag());
        for (const auto& p2 : proto_items.song_items_pairs()) {
            Song song = _get_song(p2.song());
            std::copy(p2.items().begin(), p2.items().end(),
                      std::back_inserter(pl.items.data[song]));
        }
        playlists.push_back(std::move(pl));
    }
}

inline void PlaylistTracker::_untrack(Playlist& pl) {
    pl.tracker.clear();
    pl.was_changed = true;
    pl.save();
}

void PlaylistTracker::untrack(Platform plat) {
    auto eq_plat = [plat](const Playlist& pl) { return pl.plat == plat; };
    auto it = std::find_if(playlists.begin(), playlists.end(), eq_plat);
    assert(it != playlists.end());

    _untrack(*it);
    playlists.erase(it);
    was_changed = true;
    if (playlists.size() < 2) {
        remove();
    }
}

void PlaylistTracker::remove() {
    fs::remove(dir() / id);
    std::for_each(playlists.begin(), playlists.end(),
                  PlaylistTracker::_untrack);
    id.clear();
    playlists.clear();
    was_changed = false;
}

void PlaylistTracker::save() {
    if (was_changed) {
        // save PlaylistTracker
        proto::PlaylistTracker proto_tracker;
        for (const Playlist& pl : playlists) {
            auto* pair = proto_tracker.add_plat_id_hash_pairs();
            pair->set_id_hash(pl.id_hash);
            pair->set_plat(_get_proto_platform(pl.plat));
        }
        auto f = ensure_bin_file<std::ofstream>(path);
        proto_tracker.SerializeToOstream(&f);
        was_changed = false;
    }

    for (Playlist& pl : playlists) {
        pl.save();
        if (!pl.items.was_changed) {
            continue;
        }

        // save PlaylistItems
        proto::PlaylistItems proto_items;
        proto_items.set_id(pl.id);
        proto_items.set_etag(pl.items.etag);
        for (const auto& [song, items] : pl.items.data) {
            auto* p = proto_items.add_song_items_pairs();
            *p->mutable_song() = _get_proto_song(song);
            for (const std::string& i : items) {
                p->add_items(i);
            }
        }
        auto f = ensure_bin_file<std::ofstream, class Types>(
            _playlist_items_dir(pl.plat) / pl.id);
        proto_items.SerializeToOstream(&f);
        pl.items.was_changed = false;
    }
}

PlaylistItemsCache::PlaylistItemsCache() {
    for (auto it : fs::directory_iterator(PlaylistTracker::dir())) {
        trackers.emplace_front(it.path().filename());
    }
}

void PlaylistItemsCache::save() {
    for (PlaylistTracker& t : trackers) {
        t.save();
    }
}

PlaylistDiff operator-(const SongCounts& lhs, const SongCounts& rhs) {
    PlaylistDiff res;
    for (const auto& [song, cnt] : lhs) {
        if (!rhs.count(song)) {
            res.added[song] = cnt;
            continue;
        }
        int cnt_diff = cnt - rhs.at(song);
        if (cnt_diff > 0) {
            res.added[song] = cnt_diff;
        } else if (cnt_diff < 0) {
            res.removed[song] = -cnt_diff;
        }
    }

    for (const auto& [song, cnt] : rhs) {
        if (!lhs.count(song)) {
            res.removed[song] = cnt;
        }
    }
    return res;
}

PlaylistDiff& PlaylistDiff::operator+=(const PlaylistDiff& rhs) {
    for (const auto& [song, cnt] : rhs.added) {
        if (!added.count(song) && !removed.count(song)) {
            added[song] = cnt;
        } else if (added.count(song)) {
            added[song] = std::max(added[song], cnt);
        } else if (removed.count(song) && cnt >= removed[song]) {
            // bias toward adding song when remove and added equally
            removed.erase(song);
            added[song] = cnt;
        }
    }

    for (const auto& [song, cnt] : rhs.removed) {
        if (!removed.count(song) && !added.count(song)) {
            removed[song] = cnt;
        } else if (removed.count(song)) {
            removed[song] = std::max(removed[song], cnt);
        } else if (added.count(song) && cnt > added[song]) {
            added.erase(song);
            removed[song] = cnt;
        }
    }
    return *this;
}

PlaylistDiff PlaylistDiff::operator-(const PlaylistDiff& rhs) const {
    std::unordered_map<Song, int> flat_diff;
    for (const auto& [s, cnt] : added) {
        flat_diff[s] = cnt;
    }

    for (const auto& [s, cnt] : removed) {
        flat_diff[s] = -cnt;
    }

    for (const auto& [s, cnt] : rhs.added) {
        if (!flat_diff.count(s)) {
            flat_diff[s] = 0;
        }
        flat_diff[s] -= cnt;
    }

    for (const auto& [s, cnt] : rhs.removed) {
        if (!flat_diff.count(s)) {
            flat_diff[s] = 0;
        }
        flat_diff[s] += cnt;
    }

    PlaylistDiff res;
    for (const auto& [song, cnt] : flat_diff) {
        if (cnt > 0) {
            res.added[song] = cnt;
        } else if (cnt < 0) {
            res.removed[song] = -cnt;
        }
    }
    return res;
}

SongCache::SongCache(Platform plat) {
    dir = fs::path(get_setting("cache_dir")) / "song" /
          platform_title_lower(plat);
    proto::PlaylistItemIdToSongMap proto_map;
    {
        auto file = ensure_bin_file<std::ifstream>(dir);
        proto_map.ParseFromIstream(&file);
    }
    for (const auto& b : proto_map.buckets()) {
        for (const auto& e : b.entries()) {
            songs[std::string(e.playlist_item_id())] = _get_song(e.song());
        }
    }
}

void SongCache::save() {
    proto::PlaylistItemIdToSongMap proto_map;
    for (std::size_t i = 0; i != NUM_BUCKETS; i++) {
        proto_map.add_buckets();
    }

    for (const auto& [item_id, song] : songs) {
        std::size_t bucket = std::hash<std::string>{}(item_id) % NUM_BUCKETS;
        auto* proto_pair = proto_map.mutable_buckets(bucket)->add_entries();
        proto_pair->set_playlist_item_id(item_id);
        *proto_pair->mutable_song() = _get_proto_song(song);
    }

    auto file = ensure_bin_file<std::ofstream>(dir);
    proto_map.SerializeToOstream(&file);
}
