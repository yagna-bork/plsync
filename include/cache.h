#ifndef GUARD_PLAYLIST_CACHE_H
#define GUARD_PLAYLIST_CACHE_H
#include "cache.pb.h"
#include "platform.h"
#include "util.h"
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <forward_list>
#include <map>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// TODO convert everything to a class for legibility

struct Song {
    std::vector<std::string> artists;
    std::string track;

    bool operator==(const Song& rhs) const {
        return artists == rhs.artists && track == rhs.track;
    }
};

#ifndef NDEBUG
inline std::ostream& operator<<(std::ostream& os, const Song& song) {
    os << song.track << " - ";
    for (int i = 0; i != song.artists.size(); i++) {
        if (i != 0) {
            os << ",";
        }
        os << song.artists[i];
    }
    return os;
}
#endif // !NDEBUG

// https://stackoverflow.com/a/27216842
// https://stackoverflow.com/a/20602159
template <> struct std::hash<Song> {
    size_t operator()(const Song& song) const {
        size_t seed = song.artists.size();
        std::hash<std::string> hasher;
        for (const std::string& artist : song.artists) {
            seed ^= hasher(artist) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed ^ hasher(song.track);
    }
};

/* This data is stored in PlaylistItemsCache, not PlaylistCache */
struct PlaylistItems {
    bool was_changed;
    std::string etag;
    std::unordered_map<Song, std::vector<std::string>> data;

    void merge(PlaylistItems&& other) {
        if (other.etag.empty()) {
            auto etag = std::move(other.etag);
            auto data = std::move(other.data);
        } else {
            was_changed = other.was_changed;
            etag = std::move(other.etag);
            data = std::move(other.data);
        }
    }
};

struct Playlist {
    std::string id;
    std::string id_hash;
    std::string title;
    // TODO remove Platform func arg when Playlist provided everywhere
    Platform plat;
    bool is_private;

    /* Stores the etag for an api response containing only this playlist. Used
     * in GET requests for caching. */
    std::string etag;

    /*
     * Stores the version specific id that's stored on a playlist resource
     * itself by a platform. This is Playlist.etag on Youtube and
     * Playlist.snapshot_id on spotify. Used to check if a Playlist has been
     * changed during update to PlaylistCache.
     */
    std::string version;
    std::size_t num_items;
    std::string tracker;
    bool was_changed;
    PlaylistItems items;

    Playlist() {}

    Playlist(std::string&& id, std::string&& etag, std::string&& version,
             std::string&& title, Platform plat, bool is_private,
             size_t num_items)
        : id(std::move(id)), etag(std::move(etag)), version(std::move(version)),
          title(std::move(title)), plat(plat), is_private(is_private),
          num_items(num_items) {
        id_hash = sha1(this->id);
    }

    void merge(Playlist&& other) {
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
};

/* playlist-tree-start */
/* id_hash and sid expected in binary format, not hex */
std::filesystem::path
playlist_tree_path_from_id_hash(const std::string& id_hash, Platform plat);
void playlist_tree_path_sid_from_id_hash(const std::string& id_hash,
                                         Platform plat,
                                         std::filesystem::path& out_path,
                                         std::string& out_sid);
std::filesystem::path playlist_tree_path_from_sid(const std::string& sid,
                                                  Platform plat);
int playlist_tree_height(Platform plat);
std::filesystem::path playlist_tree_add(const std::string& id_hash,
                                        Platform plat);
void playlist_tree_remove(const std::string& id_hash, Platform plat);

namespace PlaylistCache {

struct Node;
struct Head;

struct PtrUnion {
    bool is_head;
    union {
        Head* head;
        Node* node;
    };
};

struct Node {
    Playlist playlist;
    Node* next;
    PtrUnion prev;
    bool was_changed;

    Node() {}
    Node(const Playlist& pl) : was_changed(true), playlist(pl) {}
    Node(const proto::CacheNode& proto_node);
};

struct Head {
    Node* next;
    bool was_changed;
    std::string etag;
};

Head* load(Platform plat);
void update(Head* head, Platform plat, const std::vector<Playlist>& playlists,
            const std::string& etag);
void save(Head* head, Platform plat);
void cleanup(Head* head, Platform plat);
inline int short_id_len(Platform plat) { return playlist_tree_height(plat); }

/* Providing an invalid id is undefined behaviour */
Node load_node_id_hash(const std::string& id_hash, Platform plat);
Node load_node_sid(const std::string& sid, Platform plat);
// TODO no need for plat in following funcs
void save_node(const Node& node, Platform plat);
void remove_node(Node& node, Platform plat);
void create_node(const Node& node, Platform plat);

bool load_head(Platform plat, Head& res);

template <bool is_const> struct Iterator {
    PtrUnion ptr;

    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::conditional_t<is_const, const Playlist, Playlist>;
    using pointer = std::conditional_t<is_const, const Playlist*, Playlist*>;
    using reference = std::conditional_t<is_const, const Playlist&, Playlist&>;

    Iterator() {
        ptr.is_head = false;
        ptr.node = nullptr;
    }
    Iterator(Head* head) {
        ptr.is_head = true;
        ptr.head = head;
    }

    reference operator*() {
        if (!is_const)
            ptr.node->was_changed = true;
        return ptr.node->playlist;
    }
    pointer operator->() {
        if (!is_const)
            ptr.node->was_changed = true;
        return &ptr.node->playlist;
    }

    bool operator==(const Iterator& rhs) {
        if (ptr.is_head || rhs.ptr.is_head) {
            return ptr.is_head && rhs.ptr.is_head;
        } else {
            return ptr.node == rhs.ptr.node;
        }
    }

    bool operator!=(const Iterator& rhs) { return !(*this == rhs); }

    Iterator& operator++() {
        ptr.node = (ptr.is_head) ? ptr.head->next : ptr.node->next;
        ptr.is_head = false;
        return *this;
    }

    Iterator operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }
};

using const_iterator = Iterator</*is_const=*/true>;
using iterator = Iterator</*is_const=*/false>;

const_iterator cbefore_begin(Head* head);
const_iterator cbegin(Head* head);
const_iterator cend();
iterator before_begin(Head* head);
iterator begin(Head* head);
iterator end();

struct Handle {
    Head* head;
    Platform plat;

    Handle(Platform plat) : head(load(plat)), plat(plat) {}

    ~Handle() { cleanup(head, plat); }

    const_iterator cbefore_begin();
    const_iterator cbegin();
    const_iterator cend();
    iterator before_begin();
    iterator begin();
    iterator end();
};

} // namespace PlaylistCache

/* playlist-diff-start */
struct PlaylistDiff;
using SongCounts = std::unordered_map<Song, unsigned int>;
PlaylistDiff operator-(const SongCounts& lhs, const SongCounts& rhs);

struct PlaylistDiff {
    SongCounts added;
    SongCounts removed;

    bool operator==(const PlaylistDiff&) const = default;
    PlaylistDiff& operator+=(const PlaylistDiff& rhs);
    PlaylistDiff operator-(const PlaylistDiff& rhs) const;
};

struct PlaylistTracker {
    std::string id;
    // a necessary evil for now
    std::vector<PlaylistCache::Node> nodes;
    bool was_changed;

    PlaylistTracker() : id(bin_to_hex(rndstr(16))) {}
    PlaylistTracker(const std::string& id);
    void untrack(Platform plat);
    void remove();
    void save();
};

struct PlaylistItemsCache {
    std::forward_list<PlaylistTracker> trackers;

    PlaylistItemsCache();
    void save();
};

/* song-cache-start */
using SongCache = std::unordered_map<std::string, Song>;
SongCache load_song_cache(Platform plat);
void save_song_cache(const SongCache& cache, Platform plat);
#endif
