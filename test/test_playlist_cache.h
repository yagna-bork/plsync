#include "../include/cache.h"
#include "../include/util.h"
#include <array>
#include <cassert>
#include <filesystem>
#include <forward_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

using PlaylistData = std::vector<std::array<std::string, 3>>;

static Playlist create_playlist(const std::string& id,
                                const std::string& version,
                                const std::string& title) {
    Playlist pl;
    pl.id = id;
    pl.id_hash = sha1(id);
    pl.version(version);
    pl.title(title);
    pl.plat = Platform::TEST;
    pl.was_changed(false);
    return pl;
}

static void seed_cache(const std::string& version, const PlaylistData& data) {
    PlaylistTree pl_tree(Platform::TEST);
    int n = data.size();
    for (int i = 0; i != n; i++) {
        proto::CacheNode node;
        auto* proto_pl = node.mutable_playlist();
        proto_pl->set_id(data[i][0]);
        proto_pl->set_id_hash(sha1(data[i][0]));
        proto_pl->set_version(data[i][1]);
        proto_pl->set_title(data[i][2]);
        proto_pl->set_plat(proto::Platform::TEST);
        node.set_next((i != n - 1) ? sha1(data[i + 1][0]) : "");
        std::ofstream f(pl_tree.add(std::string(proto_pl->id_hash())),
                        std::ios::binary);
        node.SerializeToOstream(&f);
    }

    proto::CacheHead head;
    head.set_etag("cache_etag1");
    head.set_next(sha1(data[0][0]));
    {
        std::ofstream f(pl_tree.head(), std::ios::binary);
        head.SerializeToOstream(&f);
    }
}

static bool assert_cache_state(const std::string& etag,
                               const PlaylistData& data) {
    PlaylistTree pl_tree(Platform::TEST);
    proto::CacheHead head;
    {
        std::ifstream f(pl_tree.head(), std::ios::binary);
        head.ParseFromIstream(&f);
    }
    if (head.etag() != etag) {
        return false;
    }

    std::string next_id_hash(head.next());
    for (const auto& pl : data) {
        if (next_id_hash.empty()) {
            return false;
        }
        proto::CacheNode node;
        std::ifstream f(pl_tree.search_id_hash(next_id_hash));
        node.ParseFromIstream(&f);
        if (node.playlist().id() != pl[0] ||
            node.playlist().version() != pl[1] ||
            node.playlist().title() != pl[2]) {
            return false;
        }
        next_id_hash = node.next();
    }
    return true;
}

namespace TestPlaylistCache {

inline void test_playlist_cache_load() {
    seed_cache("cache_etag1", {{"id1", "version1", "title1"},
                               {"id2", "version2", "title2"},
                               {"id3", "version3", "title3"}});
    PlaylistCache cache(Platform::TEST);

    std::forward_list<Playlist> expected_cache = {
        create_playlist("id1", "version1", "title1"),
        create_playlist("id2", "version2", "title2"),
        create_playlist("id3", "version3", "title3")};
    bool pass = (expected_cache == cache.playlists()) &&
                (cache.etag() == "cache_etag1");
    if (pass) {
        std::cout << "test_playlist_cache_load(): PASSED\n";
    } else {
        std::cout << "test_playlist_cache_load(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void test_playlist_cache_save() {
    PlaylistCache cache(Platform::TEST);
    cache.mutable_playlists() = {create_playlist("id1", "version1", "title1"),
                                 create_playlist("id2", "version2", "title2"),
                                 create_playlist("id3", "version3", "title3")};
    cache.etag("cache_etag1");

    for (Playlist& pl : cache.mutable_playlists()) {
        pl.was_changed(true);
    }
    cache.save();

    PlaylistData expected_cache = {{"id1", "version1", "title1"},
                                   {"id2", "version2", "title2"},
                                   {"id3", "version3", "title3"}};
    if (assert_cache_state("cache_etag1", expected_cache)) {
        std::cout << "test_playlist_cache_save(): PASSED\n";
    } else {
        std::cout << "test_playlist_cache_save(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void test_update_to_empty_playlist_cache() {
    std::vector<Playlist> mod_playlists = {
        create_playlist("id1", "version1", "title1"),
        create_playlist("id2", "version2", "title2"),
        create_playlist("id3", "version3", "title3")};
    PlaylistCache cache(Platform::TEST);
    cache.update(std::move(mod_playlists), "cache_etag1");
    cache.save();

    PlaylistData expected_cache = {{"id1", "version1", "title1"},
                                   {"id2", "version2", "title2"},
                                   {"id3", "version3", "title3"}};
    bool pass = assert_cache_state("cache_etag1", expected_cache);
    if (pass) {
        std::cout << "test_update_to_empty_playlist_cache(): PASSED\n";
    } else {
        std::cout << "test_update_to_empty_playlist_cache(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void test_update_to_non_empty_playlist_cache() {
    seed_cache("cache_etag1", {{"id1", "version1", "title1"},
                               {"id2", "version2", "title2"},
                               {"id3", "version3", "title3"}});

    std::vector<Playlist> mod_playlists = {
        create_playlist("id1", "version1", "title1"),
        create_playlist("id2", "version2", "title2"),
        create_playlist("id3", "version3", "title3"),
        create_playlist("id4", "version4", "title4"),
        create_playlist("id5", "version5", "title5")};
    PlaylistCache cache(Platform::TEST);
    cache.update(std::move(mod_playlists), "cache_etag2");
    cache.save();

    PlaylistData expected_cache = {{"id1", "version1", "title1"},
                                   {"id2", "version2", "title2"},
                                   {"id3", "version3", "title3"},
                                   {"id4", "version4", "title4"},
                                   {"id5", "version5", "title5"}};
    bool pass = assert_cache_state("cache_etag2", expected_cache);
    if (pass) {
        std::cout << "test_update_to_non_empty_playlist_cache(): PASSED\n";
    } else {
        std::cout << "test_update_to_non_empty_playlist_cache(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void test_update_with_deletions() {
    seed_cache("cache_etag1", {{"id1", "version1", "title1"},
                               {"id2", "version2", "title2"},
                               {"id3", "version3", "title3"}});

    std::vector<Playlist> mod_playlists = {
        create_playlist("id3", "version3", "title3"),
        create_playlist("id4", "version4", "title4"),
        create_playlist("id5", "version5", "title5")};
    PlaylistCache cache(Platform::TEST);
    cache.update(std::move(mod_playlists), "cache_etag2");
    cache.save();

    PlaylistData expected_cache = {{"id3", "version3", "title3"},
                                   {"id4", "version4", "title4"},
                                   {"id5", "version5", "title5"}};
    bool pass = assert_cache_state("cache_etag2", expected_cache);
    if (pass) {
        std::cout << "test_update_with_deletions(): PASSED\n";
    } else {
        std::cout << "test_update_with_deletions(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void test_playlist_cache_save_sorted() {
    seed_cache("cache_etag1", {{"id3", "version3", "title3"},
                               {"id1", "version1", "title1"},
                               {"id2", "version2", "title2"}});
    PlaylistCache cache(Platform::TEST);
    cache.mutable_playlists().sort();
    cache.save();

    bool pass =
        assert_cache_state("cache_etag1", {{"id1", "version1", "title1"},
                                           {"id2", "version2", "title2"},
                                           {"id3", "version3", "title3"}});
    if (pass) {
        std::cout << "test_playlist_cache_save_sorted(): PASSED\n";
    } else {
        std::cout << "test_playlist_cache_save_sorted(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void run() {
    test_playlist_cache_load();
    test_playlist_cache_save();
    test_update_to_empty_playlist_cache();
    test_update_to_non_empty_playlist_cache();
    test_update_with_deletions();
    test_playlist_cache_save_sorted();
}

} // namespace TestPlaylistCache
