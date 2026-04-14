#include "../include/cache.h"
#include "../include/cache.pb.h"
#include "../include/new_api.h"
#include "../include/util.h"
#include <algorithm>
#include <cassert>
#include <deque>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

inline fs::path get_ensure_root(Platform plat) {
    fs::path root = fs::path(get_setting("cache_dir")) / "playlist_tree" /
                    platform_title_lower(plat);
    fs::create_directories(root);
    return root;
}

inline bool is_leaf(const fs::directory_iterator& it) {
    return it != fs::end(it) && it->is_regular_file();
}

void tree_search(const std::string& id_hash, fs::path dir, int depth,
                 fs::path& out_path, std::string& out_sid) {
    auto it = fs::directory_iterator(dir);
    if (is_leaf(it)) {
        assert(it->path().filename() == bin_to_hex(id_hash));
        out_path = it->path();
        out_sid = std::string(id_hash.begin(), id_hash.begin() + depth);
        return;
    } else {
        assert(fs::exists(dir / bin_to_hex(id_hash[depth], /*upper=*/false)));
        tree_search(id_hash, dir / bin_to_hex(id_hash[depth], /*upper=*/false),
                    depth + 1, out_path, out_sid);
    }
}

fs::path playlist_tree_path_from_id_hash(const std::string& id_hash,
                                         Platform plat) {
    std::string _;
    fs::path path;
    playlist_tree_path_sid_from_id_hash(id_hash, plat, path, _);
    return path;
}

void playlist_tree_path_sid_from_id_hash(const std::string& id_hash,
                                         Platform plat, fs::path& out_path,
                                         std::string& out_sid) {
    return tree_search(id_hash, get_ensure_root(plat), /*depth=*/0, out_path,
                       out_sid);
}

fs::path playlist_tree_path_from_sid(const std::string& sid, Platform plat) {
    fs::path path = get_ensure_root(plat);
    for (char c : sid) {
        path /= bin_to_hex(c, /*upper=*/false);
    }
    auto it = fs::directory_iterator(path);
    assert(is_leaf(it));
    return it->path();
}

fs::path tree_add(fs::path dir, const std::string& id_hash, int depth) {
    auto it = fs::directory_iterator(dir);
    if (is_leaf(it)) {
        fs::path leaf = it->path();
        std::string other_id_hash_hex = leaf.filename().string();
        std::string byte_hex(other_id_hash_hex.data() + 2 * depth, 2);
        fs::path subdir = dir / byte_hex;
        fs::create_directory(subdir);
        fs::rename(leaf, subdir / other_id_hash_hex);
        return tree_add(dir, id_hash, depth);
    }

    fs::path subdir = dir / bin_to_hex(id_hash[depth], /*upper=*/false);
    if (fs::exists(subdir)) {
        return tree_add(subdir, id_hash, depth + 1);
    }

    fs::path path = subdir / bin_to_hex(id_hash);
    ensure_bin_file<std::ofstream>(path);
    return path;
}

fs::path playlist_tree_add(const std::string& id_hash, Platform plat) {
    return tree_add(get_ensure_root(plat), id_hash, /*depth=*/0);
}

void trim_path(fs::path dir, Platform plat) {
    if (dir == get_ensure_root(plat)) {
        return;
    }

    auto it = fs::directory_iterator(dir);
    if (++it != fs::end(it)) {
        // there is more than one subtree, path can't be trimmed further
        return;
    }

    it = fs::directory_iterator(dir);
    auto sub_it = fs::directory_iterator(it->path());
    if (!is_leaf(sub_it)) {
        // the other subtree has nested subtrees, path can't be trimmed further
        return;
    }
    // trim path by moving leaf up one level into dir
    fs::rename(sub_it->path(), dir / sub_it->path().filename());
    fs::remove(it->path());
    trim_path(dir.parent_path(), plat);
}

void playlist_tree_remove(const std::string& id_hash, Platform plat) {
    fs::path leaf = playlist_tree_path_from_id_hash(id_hash, plat);
    assert(fs::exists(leaf));
    fs::remove(leaf);
    fs::remove(leaf.parent_path());
    trim_path(leaf.parent_path().parent_path(), plat);
}

namespace PlaylistCache {

static inline fs::path playlist_cache_dir(Platform plat) {
    fs::path dir = get_setting("cache_dir");
    return dir / "playlist" / platform_title_lower(plat);
}

static inline fs::path head_path(Platform plat) {
    return playlist_cache_dir(plat) / "HEAD";
}

static inline fs::path node_path(const Node* node, Platform plat) {
    assert(node && !node->playlist.id.empty());
    return playlist_cache_dir(plat) / node->playlist.id;
}

static inline void set_next(proto::CacheHead& proto_head, Node* node) {
    std::string next = node ? node->playlist.id : "";
    proto_head.set_next(next);
}

static inline void set_next(proto::CacheNode& proto_node, Node* node) {
    std::string next = node ? node->playlist.id : "";
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
    pl.short_id = std::string(proto_pl.short_id());
    pl.etag = std::string(proto_pl.etag());
    pl.version = std::string(proto_pl.version());
    pl.title = std::string(proto_pl.title());
    pl.is_private = proto_pl.is_private();
    pl.items = proto_pl.items();
}

static proto::CacheNode create_proto_node(const Node* node) {
    proto::CacheNode proto_node;
    proto_node.set_items_id(node->items_id);

    auto proto_pl = proto_node.mutable_playlist();
    proto_pl->set_id(node->playlist.id);
    proto_pl->set_id_hash(node->playlist.id_hash);
    proto_pl->set_short_id(node->playlist.short_id);
    proto_pl->set_etag(node->playlist.etag);
    proto_pl->set_version(node->playlist.version);
    proto_pl->set_title(node->playlist.title);
    proto_pl->set_is_private(node->playlist.is_private);
    proto_pl->set_items(node->playlist.items);
    return proto_node;
}

Node::Node(const proto::CacheNode& proto_node)
    : items_id(proto_node.items_id()), was_changed(false) {
    update_playlist(playlist, proto_node.playlist());
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
        std::ifstream f(playlist_cache_dir(plat) / next_id, std::ios::binary);
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
        fs::remove(node_path(tmp, plat));
        delete tmp;
    }

    // case 4: playlist not found in cache, new playlist
    curr = prev;
    for (std::size_t i = 0; i != n; i++) {
        if (!is_new[i]) {
            continue;
        }

        Node* new_node = new Node(std::move(playlists[i]));
        std::string sid(new_node->playlist.id_hash.data(), head->sid_len);
        new_node->playlist.short_id = std::move(sid);
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

void update_short_ids(Head* head, std::size_t sid_len) {
    head->sid_len = sid_len;
    head->was_changed = true;
    for (auto it = begin(head); it != end(); ++it) {
        std::string sid(it->id_hash.begin(), it->id_hash.begin() + sid_len);
        it->short_id = std::move(sid);
    }
}

Node load_node(const std::string& id, Platform plat) {
    proto::CacheNode proto_node;
    std::ifstream f(playlist_cache_dir(plat) / id, std::ios::binary);
    proto_node.ParseFromIstream(&f);
    Node node(proto_node);
    return node;
}

void save_node(const Node& node, Platform plat) {
    proto::CacheNode tmp;
    auto proto_node = create_proto_node(&node);
    std::fstream f(playlist_cache_dir(plat) / node.playlist.id,
                   std::ios::binary | std::ios::out | std::ios::in);
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
        std::ifstream file(playlist_cache_dir(plat) / node.playlist.id);
        tmp.ParseFromIstream(&file);
    }
    std::string prev(tmp.prev());
    std::string next(tmp.next());
    fs::remove(playlist_cache_dir(plat) / node.playlist.id);

    if (prev == "HEAD") {
        proto::CacheHead head;
        std::fstream file(playlist_cache_dir(plat) / prev);
        head.ParseFromIstream(&file);
        head.set_next(next);
        file.clear();
        file.seekp(0);
        head.SerializeToOstream(&file);
    } else {
        std::fstream file(playlist_cache_dir(plat) / prev);
        tmp.ParseFromIstream(&file);
        tmp.set_next(next);
        file.clear();
        file.seekp(0);
        tmp.SerializeToOstream(&file);
    }

    if (!next.empty()) {
        std::fstream file(playlist_cache_dir(plat) / next);
        tmp.ParseFromIstream(&file);
        tmp.set_prev(prev);
        file.clear();
        file.seekp(0);
        tmp.SerializeToOstream(&file);
    }
    assert(!node.playlist.short_id.empty());
    remove_sid_to_id_entry(node.playlist.short_id, plat);
    node = Node();
}

// TODO new sid need to registered, need to recalculate sid_len too
// might as well load in whole cache, add node, and recalulate sid_len
// then save entire cache again
void create_node(const Node& node, Platform plat) {
    proto::CacheHead head;
    std::string next;
    {
        auto file = ensure_bin_file<std::fstream>(head_path(plat),
                                                  std::ios::in | std::ios::out);
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
    std::fstream file(playlist_cache_dir(plat) / next,
                      std::ios::binary | std::ios::in | std::ios::out);
    proto_node.ParseFromIstream(&file);
    proto_node.set_prev(node.playlist.id);
    proto_node.SerializeToOstream(&file);
}

bool load_head(Platform plat, Head& res) {
    if (!fs::exists(head_path(plat)))
        return false;
    proto::CacheHead head;
    std::ifstream file(head_path(plat), std::ios::binary);
    head.ParseFromIstream(&file);
    res.etag = std::string(head.etag());
    res.sid_len = head.sid_len();
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

static inline fs::path playlist_items_cache_dir() {
    return fs::path(get_setting("cache_dir")) / "playlist_items";
}

static proto::Platform get_proto_platform(Platform plat) {
    switch (plat) {
    case Platform::YOUTUBE:
        return proto::Platform::YOUTUBE;
    case Platform::SPOTIFY:
        return proto::Platform::SPOTIFY;
    default:
        return proto::Platform::INVALID;
    }
}

static Platform get_platform(proto::Platform plat) {
    switch (plat) {
    case proto::Platform::YOUTUBE:
        return Platform::YOUTUBE;
    case proto::Platform::SPOTIFY:
        return Platform::SPOTIFY;
    default:
        return Platform::INVALID;
    }
}

static proto::PlaylistItems get_proto_pl_items(const fs::path& path) {
    proto::PlaylistItems proto_items;
    {
        auto file = ensure_bin_file<std::ifstream>(path);
        proto_items.ParseFromIstream(&file);
    }
    return proto_items;
}

static Song get_song(const proto::Song& proto_song) {
    Song song;
    for (const auto& artist : proto_song.artists()) {
        song.artists.emplace_back(artist);
    }
    song.track = std::string(proto_song.track());
    return song;
}

static proto::Song get_proto_song(const Song& song) {
    proto::Song proto_song;
    for (const std::string& artist : song.artists) {
        proto_song.add_artists(artist);
    }
    proto_song.set_track(song.track);
    return proto_song;
}

static PlaylistItems load_from_path(const fs::path& path) {
    PlaylistItems pl_items;
    auto proto_pl_items = get_proto_pl_items(path);
    for (const auto& p : proto_pl_items.tracked()) {
        pl_items.tracked.emplace_back(
            get_platform(p.plat()),
            Playlist(std::string(p.playlist().id()),
                     std::string(p.playlist().items_etag())));
    }
    for (const auto& song_cnt_pair : proto_pl_items.song_cnt_pairs()) {
        pl_items.song_counts[get_song(song_cnt_pair.song())] =
            song_cnt_pair.cnt();
    }
    pl_items.id = path.filename();
    return pl_items;
}

PlaylistDiff operator-(SongHashCounts lhs, SongHashCounts rhs) {
    auto it = lhs.begin();
    while (it != lhs.end()) {
        const size_t& hash = it->first;
        int& count = it->second;
        if (!rhs.count(hash) || rhs[hash] == 0) {
            ++it;
            continue;
        }

        int min_count = std::min(count, rhs[hash]);
        count -= min_count;
        rhs[hash] -= min_count;

        if (rhs[hash] == 0) {
            rhs.erase(hash);
        }
        if (count == 0) {
            it = lhs.erase(it);
        } else {
            ++it;
        }
    }
    return {std::move(lhs), std::move(rhs)};
}

PlaylistDiff& PlaylistDiff::operator+=(const PlaylistDiff& rhs) {
    for (const auto& [hash, count] : rhs.added) {
        if (added.count(hash)) {
            added[hash] = std::max(added[hash], count);
        } else if (removed.count(hash)) {
            if (removed[hash] < count) {
                removed.erase(hash);
                added[hash] = count;
            }
        } else {
            added[hash] = count;
        }
    }

    for (const auto& [hash, count] : rhs.removed) {
        if (removed.count(hash)) {
            removed[hash] = std::max(removed[hash], count);
        } else if (added.count(hash)) {
            if (added[hash] < count) {
                added.erase(hash);
                removed[hash] = count;
            }
        } else {
            removed[hash] = count;
        }
    }
    return *this;
}

PlaylistDiff PlaylistDiff::operator-(const PlaylistDiff& rhs) const {
    std::unordered_map<size_t, int> flat_diff;
    for (const auto& [hash, count] : added) {
        flat_diff[hash] = count;
    }
    for (const auto& [hash, count] : removed) {
        flat_diff[hash] = -count;
    }

    for (const auto& [hash, count] : rhs.added) {
        if (!flat_diff.count(hash)) {
            flat_diff[hash] = 0;
        }
        flat_diff[hash] -= count;
    }
    for (const auto& [hash, count] : rhs.removed) {
        if (!flat_diff.count(hash)) {
            flat_diff[hash] = 0;
        }
        flat_diff[hash] += count;
    }

    PlaylistDiff res;
    for (const auto& [hash, count] : flat_diff) {
        if (count > 0) {
            res.added[hash] = count;
        } else if (count < 0) {
            res.removed[hash] = -count;
        }
    }
    return res;
}

PlaylistItems load_playlist_items(const std::string& id) {
    return load_from_path(playlist_items_cache_dir() / id);
}

PlaylistItemsCache load_playlist_items_cache() {
    PlaylistItemsCache cache;
    for (const auto& path :
         fs::directory_iterator(playlist_items_cache_dir())) {
        cache.push_front(load_from_path(path.path()));
    }
    return cache;
}

void update_playlist_items_cache(
    PlaylistItemsCache& cache, std::shared_ptr<CURL> curl,
    const std::vector<std::string>& plat_to_access_tkn) {
    auto prev = cache.before_begin();
    auto curr = cache.begin();
    while (curr != cache.end()) {
        int i = 0;
        while (i != curr->tracked.size()) {
            auto& [plat, pl] = curr->tracked[i];
            auto node = PlaylistCache::load_node(pl.id, plat);
            pl.title = node.playlist.title;
            pl.short_id = node.playlist.short_id;

            const auto& access_tkn = plat_to_access_tkn[plat];
            Playlist modified_playlist;
            if (API::get_playlist(plat, curl.get(), access_tkn, pl.id,
                                  node.playlist.etag, modified_playlist)) {
                if (modified_playlist.id.empty()) {
                    // playlist was deleted
                    remove_node(node, plat);
                    curr->tracked.erase(curr->tracked.begin() + i);
                    curr->was_changed = true;
                    continue;
                } else {
                    pl.title = modified_playlist.title;
                    node.playlist = std::move(modified_playlist);
                    save_node(node, plat);
                }
            }
            i++;
        }

        if (curr->tracked.size() < 2) {
            curr = cache.erase_after(prev);
        } else {
            curr++;
            prev++;
        }
    }
}

void save_playlist_items_cache(const PlaylistItemsCache& cache) {
    std::unordered_set<std::string> deleted_ids;
    for (const auto& e : fs::directory_iterator(playlist_items_cache_dir())) {
        deleted_ids.insert(e.path().filename());
    }

    for (const auto& pl_items : cache) {
        deleted_ids.erase(pl_items.id);
        if (!pl_items.was_changed) {
            continue;
        }
        save_playlist_items(pl_items);
    }

    for (const auto& id : deleted_ids) {
        remove_playlist_items(id);
    }
}

void save_playlist_items(const PlaylistItems& pl_items) {
    proto::PlaylistItems proto_items;
    for (const auto& [plat, pl] : pl_items.tracked) {
        auto* pair = proto_items.add_tracked();
        pair->set_plat(get_proto_platform(plat));
        pair->mutable_playlist()->set_id(pl.id);
        pair->mutable_playlist()->set_items_etag(pl.items_etag);
    }
    for (const auto& [song, count] : pl_items.song_counts) {
        auto* pair = proto_items.add_song_cnt_pairs();
        *pair->mutable_song() = get_proto_song(song);
        pair->set_cnt(count);
    }
    auto file = ensure_bin_file<std::ofstream>(playlist_items_cache_dir() /
                                               pl_items.id);
    proto_items.SerializeToOstream(&file);
}

void remove_playlist_items(const std::string& id) {
    auto pl_items = get_proto_pl_items(playlist_items_cache_dir() / id);
    for (const auto& p : pl_items.tracked()) {
        Platform plat = get_platform(p.plat());
        auto node =
            PlaylistCache::load_node(std::string(p.playlist().id()), plat);
        if (node.playlist.id.empty()) {
            continue;
        }
        node.items_id.clear();
        PlaylistCache::save_node(node, plat);
    }
    fs::remove(playlist_items_cache_dir() / id);
}

std::size_t NUM_BUCKETS = 500;

using Map = SidToIdMap;
using ProtoMap = proto::SidToIdMap;
namespace Cache = PlaylistCache;

fs::path sid_to_id_map_dir(Platform plat) {
    return fs::path(get_setting("cache_dir")) / "sid_to_id_map" /
           platform_title_lower(plat);
}

Map load_sid_to_id_map(Platform plat) {
    ProtoMap proto_map;
    {
        auto file = ensure_bin_file<std::ifstream>(sid_to_id_map_dir(plat));
        proto_map.ParseFromIstream(&file);
    }

    Map map;
    for (const auto& bucket : proto_map.buckets()) {
        for (const auto& pair : bucket.pairs()) {
            std::string short_id(pair.short_id());
            std::string id(pair.id());
            map[short_id] = id;
        }
    }
    return map;
}

std::string sid_to_id_lookup(const std::string& sid, Platform plat) {
    ProtoMap proto_map;
    {
        auto file = ensure_bin_file<std::ifstream>(sid_to_id_map_dir(plat));
        proto_map.ParseFromIstream(&file);
    }

    if (proto_map.buckets_size() == 0) {
        throw SidOutOfRangeError();
    }

    auto bucket = std::hash<std::string>{}(sid) % NUM_BUCKETS;
    for (const auto& pair : proto_map.buckets(bucket).pairs()) {
        if (pair.short_id() != sid) {
            continue;
        }
        return std::string(pair.id());
    }
    throw SidOutOfRangeError();
}

void remove_sid_to_id_entry(const std::string& sid, Platform plat) {
    ProtoMap proto_map;
    auto file = ensure_bin_file<std::fstream>(sid_to_id_map_dir(plat),
                                              std::ios::in | std::ios::out);
    proto_map.ParseFromIstream(&file);
    auto bucket_idx = std::hash<std::string>{}(sid) % NUM_BUCKETS;
    auto* proto_bucket = proto_map.mutable_buckets(bucket_idx);
    for (int i = 0; i != proto_bucket->pairs_size(); i++) {
        if (proto_bucket->pairs(i).short_id() != sid) {
            continue;
        }
        proto_bucket->mutable_pairs()->DeleteSubrange(i, 1);
        proto_map.SerializeToOstream(&file);
        return;
    }
    throw SidOutOfRangeError();
}

Map update_sid_to_id_map(Cache::Head* head, Platform plat) {
    Map map;
    for (auto it = Cache::cbegin(head); it != Cache::cend(); ++it) {
        map[it->short_id] = it->id;
    }
    save_sid_to_id_map(map, plat);
    return map;
}

void save_sid_to_id_map(const Map& map, Platform plat) {
    ProtoMap proto_map;
    for (std::size_t i = 0; i != NUM_BUCKETS; i++) {
        proto_map.add_buckets();
    }

    for (const auto& pair : map) {
        auto short_id_hash = std::hash<std::string>{}(pair.first);
        std::size_t bucket = short_id_hash % NUM_BUCKETS;
        auto* proto_pair = proto_map.mutable_buckets(bucket)->add_pairs();
        proto_pair->set_short_id(pair.first);
        proto_pair->set_id(pair.second);
    }

    auto file = ensure_bin_file<std::ofstream>(sid_to_id_map_dir(plat));
    proto_map.SerializeToOstream(&file);
}

static inline fs::path song_cache_dir(Platform plat) {
    return fs::path(get_setting("cache_dir")) / "song_cache" /
           platform_title_lower(plat);
}

SongCache load_song_cache(Platform plat) {
    proto::PlaylistItemIdToSongMap proto_map;
    {
        auto file = ensure_bin_file<std::ifstream>(song_cache_dir(plat));
        proto_map.ParseFromIstream(&file);
    }

    SongCache cache;
    for (const auto& bucket : proto_map.buckets()) {
        for (const auto& e : bucket.entries()) {
            cache[std::string(e.playlist_item_id())] = get_song(e.song());
        }
    }
    return cache;
}

void save_song_cache(const SongCache& cache, Platform plat) {
    proto::PlaylistItemIdToSongMap proto_map;
    for (std::size_t i = 0; i != NUM_BUCKETS; i++) {
        proto_map.add_buckets();
    }

    for (const auto& [item_id, song] : cache) {
        std::size_t bucket = std::hash<std::string>{}(item_id) % NUM_BUCKETS;
        auto* proto_pair = proto_map.mutable_buckets(bucket)->add_entries();
        proto_pair->set_playlist_item_id(item_id);
        *proto_pair->mutable_song() = get_proto_song(song);
    }

    auto file = ensure_bin_file<std::ofstream>(song_cache_dir(plat));
    proto_map.SerializeToOstream(&file);
}
