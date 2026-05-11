// TODO cleanup all includes
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
#include <vector>

namespace fs = std::filesystem;

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

PlaylistTree::PlaylistTree(Platform plat) {
    root = fs::path(get_setting("cache_dir")) / "playlist" /
           platform_title_lower(plat);
    fs::create_directories(root);
}

fs::path PlaylistTree::head() { return root / "HEAD"; }

bool PlaylistTree::_is_leaf(const fs::directory_iterator& it) {
    return (it != fs::end(it)) && (it->is_regular_file()) &&
           (it->path() != head());
}

void PlaylistTree::_search(const std::string& id_hash, fs::path dir, int depth,
                           fs::path& out_path, std::string& out_sid) {
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

fs::path PlaylistTree::search_id_hash(const std::string& id_hash) {
    fs::path path;
    std::string _;
    _search(id_hash, root, 0, path, _);
    return path;
}

fs::path PlaylistTree::search_sid(const std::string& sid) {
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

int PlaylistTree::_height(const fs::path& dir) {
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

int PlaylistTree::height() { return _height(root); }

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

static proto::Platform _get_proto_platform(Platform plat) {
    switch (plat) {
    case Platform::YOUTUBE:
        return proto::Platform::YOUTUBE;
    case Platform::SPOTIFY:
        return proto::Platform::SPOTIFY;
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
    default:
        return Platform::INVALID;
    }
}

namespace PlaylistCache {

static inline void set_next(proto::CacheHead& proto_head, Node* node) {
    std::string next = node ? node->playlist.id_hash : "";
    proto_head.set_next(next);
}

static inline void set_next(proto::CacheNode& proto_node, Node* node) {
    std::string next = node ? node->playlist.id_hash : "";
    proto_node.set_next(next);
}

static void set_next(const_iterator it, Node* next) {
    if (it.ptr.is_head) {
        it.ptr.head->next = next;
        it.ptr.head->was_changed = true;
    } else {
        it.ptr.node->next = next;
        it.ptr.node->was_changed = true;
    }
}

static void set_prev(Node* node, Head* head) {
    node->prev.is_head = true;
    node->prev.head = head;
}

static void set_prev(Node* node, Node* prev) {
    node->prev.is_head = false;
    node->prev.node = prev;
}

static inline void free_and_advance_node(Node** ptr) {
    Node* tmp = *ptr;
    *ptr = (*ptr)->next;
    delete tmp;
}

static void update_playlist(Playlist& pl, const proto::Playlist& proto_pl) {
    pl.id = std::string(proto_pl.id());
    pl.id_hash = std::string(proto_pl.id_hash());
    pl.title = std::string(proto_pl.title());
    pl.plat = _get_platform(proto_pl.plat());
    pl.is_private = proto_pl.is_private();
    pl.etag = std::string(proto_pl.etag());
    pl.version = std::string(proto_pl.version());
    pl.num_items = proto_pl.num_items();
    pl.tracker = proto_pl.tracker();
}

static proto::CacheNode create_proto_node(const Node* node) {
    proto::CacheNode proto_node;
    auto* proto_pl = proto_node.mutable_playlist();
    proto_pl->set_id(node->playlist.id);
    proto_pl->set_id_hash(node->playlist.id_hash);
    proto_pl->set_title(node->playlist.title);
    proto_pl->set_plat(_get_proto_platform(node->playlist.plat));
    proto_pl->set_is_private(node->playlist.is_private);
    proto_pl->set_etag(node->playlist.etag);
    proto_pl->set_version(node->playlist.version);
    proto_pl->set_num_items(node->playlist.num_items);
    proto_pl->set_tracker(node->playlist.tracker);
    return proto_node;
}

Node::Node(const proto::CacheNode& proto_node) : was_changed(false) {
    update_playlist(playlist, proto_node.playlist());
}

const_iterator cbefore_begin(Head* head) { return const_iterator(head); }
const_iterator cbegin(Head* head) { return ++const_iterator(head); }
const_iterator cend() { return const_iterator(); }
iterator before_begin(Head* head) { return iterator(head); }
iterator begin(Head* head) { return ++iterator(head); }
iterator end() { return iterator(); }

Head* load(Platform plat) {
    PlaylistTree tree(plat);
    proto::CacheHead proto_head;
    {
        auto f = ensure_bin_file<std::ifstream>(tree.head());
        proto_head.ParseFromIstream(&f);
    }
    Head* head = new Head;
    head->etag = proto_head.etag();
    std::string next(proto_head.next());
    if (next.empty()) {
        return head;
    }

    auto dummy_head = new Node;
    auto node = dummy_head;
    while (!next.empty()) {
        proto::CacheNode proto_node;
        std::ifstream f(tree.search_id_hash(next), std::ios::binary);
        proto_node.ParseFromIstream(&f);

        auto new_node = new Node(proto_node);
        node->next = new_node;
        set_prev(new_node, node);
        next = std::string(proto_node.next());
        node = new_node;
    }
    head->next = dummy_head->next;
    delete dummy_head;
    set_prev(head->next, head);
    return head;
}

void save(Head* head, Platform plat) {
    // save and delete head
    PlaylistTree tree(plat);
    if (head->was_changed) {
        proto::CacheHead proto_head;
        set_next(proto_head, head->next);
        proto_head.set_etag(head->etag);
        std::ofstream f(tree.head(), std::ios::binary);
        proto_head.SerializeToOstream(&f);
        head->was_changed = false;
    }

    // save nodes
    Node* node = head->next;
    std::string prev = "HEAD";
    while (node) {
        if (!node->was_changed) {
            node = node->next;
            continue;
        }

        auto proto_node = create_proto_node(node);
        set_next(proto_node, node->next);
        proto_node.set_prev(prev);
        fs::path p = tree.search_id_hash(node->playlist.id_hash);
        if (p.empty()) {
            p = tree.add(node->playlist.id_hash);
        }
        std::ofstream f(p, std::ios::binary);
        proto_node.SerializeToOstream(&f);
        node->was_changed = false;

        prev = node->playlist.id_hash;
        node = node->next;
    }
}

void cleanup(Head* head, Platform plat) {
    Node* node = head->next;
    delete head;
    while (node) {
        free_and_advance_node(&node);
    }
}

void update(Head* head, Platform plat, const std::vector<Playlist>& playlists,
            const std::string& etag) {
    std::size_t n = playlists.size();
    std::deque<bool> is_new(n, true);
    std::unordered_map<std::string, std::size_t> version_to_idx;
    std::unordered_map<std::string, std::size_t> id_to_idx;
    for (std::size_t i = 0; i != n; i++) {
        version_to_idx[playlists[i].version] = i;
        id_to_idx[playlists[i].id] = i;
    }

    PlaylistTree tree(plat);
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
            ++prev;
            ++curr;
            ++curr_write;
            continue;
        }

        // case 2: version changed but id found, playlist changed
        std::string id(curr->id);
        if (id_to_idx.count(id)) {
            std::size_t i = id_to_idx[id];
            is_new[i] = false;
            *curr_write = playlists[i];
            ++prev;
            ++curr;
            ++curr_write;
            continue;
        }

        // case 3: id not found, playlist deleted
        Node* tmp = curr.ptr.node;
        ++curr;
        ++curr_write;
        set_next(prev, tmp->next);
        if (tmp->next) {
            tmp->next->prev = tmp->prev;
            tmp->next->was_changed = true;
        }
        tree.erase(tmp->playlist.id_hash);
        delete tmp;
    }

    // case 4: playlist not found in cache, new playlist
    curr = prev;
    for (std::size_t i = 0; i != n; i++) {
        if (!is_new[i]) {
            continue;
        }

        Node* new_node = new Node(std::move(playlists[i]));
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
        id_hashes.emplace_back(node->playlist.id_hash);
        node = node->next;
    }

    std::vector<std::size_t> collision_idxs(id_hashes.size());
    std::iota(collision_idxs.begin(), collision_idxs.end(), 0);

    std::unordered_map<std::string, std::vector<std::size_t>> sid_groups;
    std::size_t sid_len = 0;
    while (!collision_idxs.empty()) {
        sid_len++;
        sid_groups.clear();
        for (std::size_t idx : collision_idxs) {
            std::string sid(id_hashes[idx].data(), sid_len);
            sid_groups[sid].push_back(idx);
        }

        collision_idxs.clear();
        for (const auto& pr : sid_groups) {
            const auto& group = pr.second;
            if (group.size() < 2) {
                continue;
            }
            std::copy(group.begin(), group.end(),
                      std::back_inserter(collision_idxs));
        }
    }
    return sid_len;
}

static Node load_node(const fs::path& path) {
    if (!fs::exists(path)) {
        return Node();
    }
    proto::CacheNode proto_node;
    std::ifstream f(path, std::ios::binary);
    proto_node.ParseFromIstream(&f);
    return Node(proto_node);
}

Node load_node_id_hash(const std::string& id_hash, Platform plat) {
    return load_node(PlaylistTree(plat).search_id_hash(id_hash));
}

Node load_node_sid(const std::string& sid, Platform plat) {
    return load_node(PlaylistTree(plat).search_sid(sid));
}

void save_node(const Node& node, Platform plat) {
    proto::CacheNode tmp;
    auto proto_node = create_proto_node(&node);
    PlaylistTree tree(plat);
    std::fstream f(tree.search_id_hash(node.playlist.id_hash),
                   std::ios::binary | std::ios::out | std::ios::in);
    tmp.ParseFromIstream(&f);
    proto_node.set_next(tmp.next());
    proto_node.set_prev(tmp.prev());
    f.clear();
    f.seekp(0);
    proto_node.SerializeToOstream(&f);
}

void remove_node(Node& node, Platform plat) {
    PlaylistTree tree(plat);
    proto::CacheNode tmp;
    {
        std::ifstream file(tree.search_id_hash(node.playlist.id_hash));
        tmp.ParseFromIstream(&file);
    }
    std::string prev(tmp.prev());
    std::string next(tmp.next());
    tree.erase(node.playlist.id_hash);

    if (prev == "HEAD") {
        proto::CacheHead head;
        std::fstream file(tree.head());
        head.ParseFromIstream(&file);
        head.set_next(next);
        file.clear();
        file.seekp(0);
        head.SerializeToOstream(&file);
    } else {
        std::fstream file(tree.search_id_hash(prev));
        tmp.ParseFromIstream(&file);
        tmp.set_next(next);
        file.clear();
        file.seekp(0);
        tmp.SerializeToOstream(&file);
    }

    if (!next.empty()) {
        std::fstream file(tree.search_id_hash(next));
        tmp.ParseFromIstream(&file);
        tmp.set_prev(prev);
        file.clear();
        file.seekp(0);
        tmp.SerializeToOstream(&file);
    }
    node = Node();
}

void create_node(const Node& node, Platform plat) {
    PlaylistTree tree(plat);
    proto::CacheHead head;
    std::string next;
    {
        auto file = ensure_bin_file<std::fstream>(tree.head(),
                                                  std::ios::in | std::ios::out);
        head.ParseFromIstream(&file);
        next = std::string(head.next());
        head.set_next(node.playlist.id_hash);
        head.SerializeToOstream(&file);
    }

    auto proto_node = create_proto_node(&node);
    proto_node.set_next(next);
    proto_node.set_prev("HEAD");
    {
        std::ofstream file(tree.add(node.playlist.id_hash), std::ios::binary);
        proto_node.SerializeToOstream(&file);
    }

    if (next.empty()) {
        return;
    }
    std::fstream file(tree.search_id_hash(next),
                      std::ios::binary | std::ios::in | std::ios::out);
    proto_node.ParseFromIstream(&file);
    proto_node.set_prev(node.playlist.id_hash);
    proto_node.SerializeToOstream(&file);
}

bool load_head(Platform plat, Head& res) {
    PlaylistTree tree(plat);
    if (!fs::exists(tree.head()))
        return false;
    proto::CacheHead head;
    std::ifstream file(tree.head(), std::ios::binary);
    head.ParseFromIstream(&file);
    res.etag = std::string(head.etag());
    return true;
}

const_iterator Handle::cbefore_begin() {
    return PlaylistCache::cbefore_begin(head);
}
const_iterator Handle::cbegin() { return PlaylistCache::cbegin(head); }
const_iterator Handle::cend() { return PlaylistCache::cend(); }
iterator Handle::before_begin() { return PlaylistCache::before_begin(head); }
iterator Handle::begin() { return PlaylistCache::begin(head); }
iterator Handle::end() { return PlaylistCache::end(); }

} // namespace PlaylistCache

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
        auto node = PlaylistCache::load_node_id_hash(std::string(p1.id_hash()),
                                                     _get_platform(p1.plat()));
        PlaylistItems& items = node.playlist.items;
        proto::PlaylistItems proto_items;
        {
            auto f = ensure_bin_file<std::ifstream>(
                _playlist_items_dir(node.playlist.plat) / node.playlist.id);
            proto_items.ParseFromIstream(&f);
        }

        items.etag = std::string(proto_items.etag());
        for (const auto& p2 : proto_items.song_items_pairs()) {
            Song song = _get_song(p2.song());
            std::copy(p2.items().begin(), p2.items().end(),
                      std::back_inserter(items.data[song]));
        }
        nodes.push_back(std::move(node));
    }
}

inline void PlaylistTracker::_untrack_node(PlaylistCache::Node& node) {
    node.playlist.tracker.clear();
    PlaylistCache::save_node(node, node.playlist.plat);
}

void PlaylistTracker::untrack(Platform plat) {
    auto eq_plat = [&plat](const PlaylistCache::Node& n) {
        return n.playlist.plat == plat;
    };
    auto node = std::find_if(nodes.begin(), nodes.end(), eq_plat);
    assert(node != nodes.end());

    _untrack_node(*node);
    nodes.erase(node);
    was_changed = true;
    if (nodes.size() < 2) {
        remove();
    }
}

void PlaylistTracker::remove() {
    fs::remove(dir() / id);
    std::for_each(nodes.begin(), nodes.end(), PlaylistTracker::_untrack_node);
    id.clear();
    nodes.clear();
    was_changed = false;
}

void PlaylistTracker::save() {
    if (was_changed) {
        // save PlaylistTracker
        proto::PlaylistTracker proto_tracker;
        for (const PlaylistCache::Node& node : nodes) {
            auto* pair = proto_tracker.add_plat_id_hash_pairs();
            pair->set_id_hash(node.playlist.id_hash);
            pair->set_plat(_get_proto_platform(node.playlist.plat));
        }
        auto f = ensure_bin_file<std::ofstream>(path);
        proto_tracker.SerializeToOstream(&f);
    }

    for (const PlaylistCache::Node& node : nodes) {
        if (node.was_changed) {
            // save Playlist
            PlaylistCache::save_node(node, node.playlist.plat);
        }
        if (!node.playlist.items.was_changed) {
            continue;
        }

        // save PlaylistItems
        const PlaylistItems& items = node.playlist.items;
        proto::PlaylistItems proto_items;
        proto_items.set_id(node.playlist.id);
        proto_items.set_etag(items.etag);
        for (const auto& [song, items] : items.data) {
            auto* p = proto_items.add_song_items_pairs();
            *p->mutable_song() = _get_proto_song(song);
            for (const std::string& i : items) {
                p->add_items(i);
            }
        }
        auto f = ensure_bin_file<std::ofstream, class Types>(
            _playlist_items_dir(node.playlist.plat) / node.playlist.id);
        proto_items.SerializeToOstream(&f);
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
