#ifndef GUARD_PLAYLIST_CACHE_H
#define GUARD_PLAYLIST_CACHE_H
#include "cache.pb.h"
#include "platform.h"
#include "util.h"
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct Playlist {
    std::string id;
    std::string id_hash;

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

    std::string title;
    bool is_private = false;
    std::size_t items = 0;
    std::string short_id;
    std::string items_id;
    std::string items_etag;

    Playlist() {}

    Playlist(std::string&& id, std::string&& etag, std::string&& version,
             std::string&& title, bool is_private, std::size_t items,
             std::string&& short_id = "")
        : id(std::move(id)), id_hash(sha1(this->id)), etag(std::move(etag)),
          version(std::move(version)), title(std::move(title)),
          is_private(is_private), items(items), short_id(short_id) {}

    Playlist(const std::string& id, const std::string& items_etag)
        : id_hash(sha1(id)), id(id), items_etag(items_etag) {}

    Playlist(const Playlist& other) = default;
    Playlist(Playlist&& other) = default;
    ~Playlist() = default;

    Playlist& operator=(const Playlist& rhs) {
        std::string tmp;
        id = (!rhs.id.empty()) ? rhs.id : id;
        id_hash = (!rhs.id_hash.empty()) ? rhs.id_hash : id_hash;
        etag = (!rhs.etag.empty()) ? rhs.etag : etag;
        version = (!rhs.version.empty()) ? rhs.version : version;
        title = (!rhs.title.empty()) ? rhs.title : title;
        short_id = (!rhs.short_id.empty()) ? rhs.short_id : short_id;
        items_id = (!rhs.items_id.empty()) ? rhs.items_id : items_id;
        items_etag = (!rhs.items_etag.empty()) ? rhs.items_etag : items_etag;
        is_private = (is_private == rhs.is_private) ? is_private : true;
        items = std::max(is_private, rhs.is_private);
        return *this;
    }

    Playlist& operator=(Playlist&& rhs) {
        std::string tmp;
        if (!rhs.id.empty()) {
            id = std::move(rhs.id);
        } else {
            tmp = std::move(rhs.id);
        }
        if (!rhs.id_hash.empty()) {
            id_hash = std::move(rhs.id_hash);
        } else {
            tmp = std::move(rhs.id_hash);
        }
        if (!rhs.etag.empty()) {
            etag = std::move(rhs.etag);
        } else {
            tmp = std::move(rhs.etag);
        }
        if (!rhs.version.empty()) {
            version = std::move(rhs.version);
        } else {
            tmp = std::move(rhs.version);
        }
        if (!rhs.title.empty()) {
            title = std::move(rhs.title);
        } else {
            tmp = std::move(rhs.title);
        }
        if (!rhs.short_id.empty()) {
            short_id = std::move(rhs.short_id);
        } else {
            tmp = std::move(rhs.short_id);
        }
        if (!rhs.items_id.empty()) {
            items_id = std::move(rhs.items_id);
        } else {
            tmp = std::move(rhs.items_id);
        }
        if (!rhs.items_etag.empty()) {
            items_etag = std::move(rhs.items_etag);
        } else {
            tmp = std::move(rhs.items_etag);
        }
        is_private = (is_private == rhs.is_private) ? is_private : true;
        items = std::max(is_private, rhs.is_private);
        return *this;
    }
};

/* playlist-tree-start */
std::filesystem::path
playlist_tree_path_from_id_hash(const std::string& id_hash, Platform plat);
void playlist_tree_path_sid_from_id_hash(const std::string& id_hash,
                                         Platform plat,
                                         std::filesystem::path& out_path,
                                         std::string& out_sid);
std::filesystem::path playlist_tree_path_from_sid(const std::string& sid,
                                                  Platform plat);
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
    std::string items_id;
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
    std::size_t sid_len;
};

Head* load(Platform plat);
void update(Head* head, Platform plat, const std::vector<Playlist>& playlists,
            const std::string& etag);
void save(Head* head, Platform plat);

/* Providing an invalid id is undefined behaviour */
Node load_node(const std::string& id, Platform plat);
void save_node(const Node& node, Platform plat);
void remove_node(Node& node, Platform plat);
void create_node(const Node& node, Platform plat);

bool load_head(Platform plat, Head& res);

/* Determine min characters of id_hash that make them all unique */
std::size_t calculate_short_id_len(Head* head);
void update_short_ids(Head* head, std::size_t short_id_len);

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
    ~Handle() { save(head, plat); }

    const_iterator cbefore_begin();
    const_iterator cbegin();
    const_iterator cend();
    iterator before_begin();
    iterator begin();
    iterator end();
};

} // namespace PlaylistCache

/* playlist-diff-start */
struct Song {
    std::vector<std::string> artists;
    std::string track;

    bool operator==(const Song& rhs) const {
        return artists == rhs.artists && track == rhs.track;
    }
};

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

using SongCounts = std::unordered_map<Song, int>;

struct PlaylistDiff;
using SongHashCounts = std::unordered_map<size_t, int>;
PlaylistDiff operator-(SongHashCounts lhs, SongHashCounts rhs);

struct PlaylistDiff {
    SongHashCounts added;
    SongHashCounts removed;

    bool operator==(const PlaylistDiff&) const = default;
    PlaylistDiff& operator+=(const PlaylistDiff& rhs);
    PlaylistDiff operator-(const PlaylistDiff& rhs) const;
};

/* playlist-items-cache-start */
struct PlaylistItems {
    std::string id;
    std::vector<std::pair<Platform, Playlist>> tracked;
    SongCounts song_counts;
    bool was_changed;
};
PlaylistItems load_playlist_items(const std::string& id);
void save_playlist_items(const PlaylistItems& pl_items);
void remove_playlist_items(const std::string& id);

using PlaylistItemsCache = std::forward_list<PlaylistItems>;
PlaylistItemsCache load_playlist_items_cache();
/* Can throw API::RequestError on failure */
void update_playlist_items_cache(
    PlaylistItemsCache& cache, std::shared_ptr<CURL> curl,
    const std::vector<std::string>& plat_to_access_token);
void save_playlist_items_cache(const PlaylistItemsCache& cache);

/* SidToIdMap start */
class SidOutOfRangeError : public std::out_of_range {
public:
    SidOutOfRangeError()
        : std::out_of_range("lookup attempted before map was initialised") {}
};

using SidToIdMap = std::unordered_map<std::string, std::string>;
SidToIdMap load_sid_to_id_map(Platform plat);
SidToIdMap update_sid_to_id_map(PlaylistCache::Head* head, Platform plat);
void save_sid_to_id_map(const SidToIdMap& map, Platform plat);

/* These functions may throw SidOutOrRangeError */
std::string sid_to_id_lookup(const std::string& sid, Platform plat);
void remove_sid_to_id_entry(const std::string& sid, Platform plat);

using SongCache = std::unordered_map<std::string, Song>;
SongCache load_song_cache(Platform plat);
void save_song_cache(const SongCache& cache, Platform plat);
#endif
