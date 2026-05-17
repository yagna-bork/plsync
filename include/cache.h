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

class Song {
public:
    std::vector<std::string> artists;
    std::string track;

    bool operator==(const Song& rhs) const;
    std::ostream& operator<<(std::ostream& os);
};

template <> struct std::hash<Song> {
    size_t operator()(const Song& song) const;
};

/* This data is stored in PlaylistItemsCache, not PlaylistCache */
class PlaylistItems {
    friend class PlaylistTracker;

public:
    void merge(PlaylistItems&& other);
    bool operator==(const PlaylistItems&) const = default;

    const std::string& etag() const;
    void etag(const std::string& etag);

    const std::unordered_map<Song, std::vector<std::string>>& data() const;
    std::unordered_map<Song, std::vector<std::string>>& mutable_data();

private:
    std::string _etag;
    std::unordered_map<Song, std::vector<std::string>> _data;
    bool _was_changed = false;
};

class Playlist {
public:
    std::string id;
    std::string id_hash;
    Platform plat = Platform::INVALID;
    PlaylistItems items;

    Playlist() : plat(Platform::INVALID) {}
    Playlist(const proto::Playlist& proto_pl);
    Playlist(std::string&& id, std::string&& etag, std::string&& version,
             std::string&& title, Platform plat, bool is_private,
             size_t num_items);

    void merge(Playlist&& other);

    /* The following are persistent operations on the underlying cache */
    static Playlist load_from_id_hash(const std::string& id_hash,
                                      Platform plat);
    static Playlist load_from_sid(const std::string& sid, Platform plat);
    void add();
    void save();
    void save(const std::string& prev_id_hash, const std::string& next_id_hash);
    void remove();

    bool operator==(const Playlist&) const = default;
    bool operator<(const Playlist&) const;

    const std::string& title() const;
    void title(const std::string& title);

    bool is_private() const;
    void is_private(bool is_private);

    const std::string& etag() const;
    void etag(const std::string& etag);

    const std::string& version() const;
    void version(const std::string& version);

    size_t num_items() const;
    void num_items(size_t num_items);

    const std::string& tracker() const;
    void tracker(const std::string& tracked);

    bool was_changed() const;
#ifndef NDEBUG
    void was_changed(bool was_changed);
#endif // !NDEBUG

private:
    std::string _title;
    bool _is_private = false;
    /* Stores the etag for an api response containing only this playlist. Used
     * in GET requests for caching. */
    std::string _etag;
    /*
     * Stores the version specific id that's stored on a playlist resource
     * itself by a platform. This is Playlist.etag on Youtube and
     * Playlist.snapshot_id on spotify. Used to check if a Playlist has been
     * changed during update to PlaylistCache.
     */
    std::string _version;
    std::size_t _num_items = 0;
    std::string _tracker;
    bool _was_changed = false;

    static Playlist _load(const std::filesystem::path& path);
    proto::CacheNode _proto_node();
    std::filesystem::path _path();
};

class PlaylistTree {
public:
    std::filesystem::path root;

    PlaylistTree(Platform plat);
    std::filesystem::path head() const;
    int height() const;

    /* id_hash and sid expected in binary format, not hex */
    std::filesystem::path search_id_hash(const std::string& id_hash) const;
    std::filesystem::path search_sid(const std::string& sid) const;
    std::filesystem::path add(const std::string& id_hash);
    void erase(const std::string& id_hash);

private:
    void _search(const std::string& id_hash, std::filesystem::path dir,
                 int depth, std::filesystem::path& out_path,
                 std::string& out_sid) const;
    bool _is_leaf(const std::filesystem::directory_iterator& it) const;
    int _height(const std::filesystem::path& dir) const;
    std::filesystem::path _add(const std::filesystem::path& dir,
                               const std::string& id_hash, int depth);
    void _trim_path(const std::filesystem::path& dir);
};

class PlaylistCache {
public:
    Platform plat;
    PlaylistTree pl_tree;

    PlaylistCache(Platform plat);
    void update(std::vector<Playlist>&& playlists, const std::string& etag);
    void save();

    static int short_id_len(Platform plat) {
        return PlaylistTree(plat).height();
    }

    const std::string& etag() const;
    void etag(const std::string& etag);

    const std::forward_list<Playlist>& playlists() const;
    std::forward_list<Playlist>& mutable_playlists();

private:
    std::string _etag;
    // TODO this has crossed the threshold to become a std::list
    std::forward_list<Playlist> _playlists;
    bool _was_changed = false;

    std::string _next_id_hash(std::forward_list<Playlist>::const_iterator it);
    bool _was_first_element_reordered(const std::string& head_next);
    bool _was_reordered(std::forward_list<Playlist>::const_iterator it,
                        const std::string& prev, const std::string& next);
};

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

class PlaylistTracker {
public:
    std::string id;
    std::filesystem::path path;
    std::vector<Playlist> playlists;

    PlaylistTracker() : id(bin_to_hex(rndstr(16))), path(dir() / id) {}
    PlaylistTracker(const std::string& id);
    void untrack(Platform plat);
    void remove();
    void save();

    static std::filesystem::path dir();

private:
    std::filesystem::path _playlist_items_dir(Platform plat);
    static void _untrack(Playlist& pl);
};

class PlaylistItemsCache {
public:
    std::forward_list<PlaylistTracker> trackers;

    PlaylistItemsCache();
    void save();
};

class SongCache {
public:
    static const int NUM_BUCKETS = 500;
    std::filesystem::path dir;
    std::unordered_map<std::string, Song> songs;

    SongCache(Platform plat);
    void save();
};
#endif
