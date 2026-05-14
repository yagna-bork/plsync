#include "../include/cache.h"
#include "../include/util.h"
#include <cassert>
#include <filesystem>
#include <forward_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

static Playlist create_playlist(const std::string& id,
                                const std::string& version) {
    Playlist pl;
    pl.id = id;
    pl.id_hash = sha1(id);
    pl.version = version;
    pl.plat = Platform::TEST;
    return pl;
}

static void seed_cache(
    const std::string& version,
    const std::vector<std::pair<std::string, std::string>>& id_version_pairs) {
    PlaylistTree pl_tree(Platform::TEST);
    int n = id_version_pairs.size();
    for (int i = 0; i != n; i++) {
        proto::CacheNode node;
        auto* proto_pl = node.mutable_playlist();
        proto_pl->set_id(id_version_pairs[i].first);
        proto_pl->set_id_hash(sha1(id_version_pairs[i].first));
        proto_pl->set_version(id_version_pairs[i].second);
        proto_pl->set_plat(proto::Platform::TEST);
        node.set_next((i != n - 1) ? sha1(id_version_pairs[i + 1].first) : "");
        std::ofstream f(pl_tree.add(std::string(proto_pl->id_hash())),
                        std::ios::binary);
        node.SerializeToOstream(&f);
    }

    proto::CacheHead head;
    head.set_etag("cache_etag1");
    head.set_next(sha1(id_version_pairs[0].first));
    {
        std::ofstream f(pl_tree.head(), std::ios::binary);
        head.SerializeToOstream(&f);
    }
}

static bool assert_cache_state(
    const std::string& etag,
    const std::vector<std::pair<std::string, std::string>>& id_version_pairs) {
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
    for (const auto& [id, version] : id_version_pairs) {
        if (next_id_hash.empty()) {
            return false;
        }
        proto::CacheNode node;
        std::ifstream f(pl_tree.search_id_hash(next_id_hash));
        node.ParseFromIstream(&f);
        if (node.playlist().id() != id ||
            node.playlist().version() != version) {
            return false;
        }
        next_id_hash = node.next();
    }
    return true;
}

namespace TestPlaylistCache {

inline void test_playlist_cache_load() {
    seed_cache("cache_etag1",
               {{"id1", "version1"}, {"id2", "version2"}, {"id3", "version3"}});
    PlaylistCache cache(Platform::TEST);

    std::forward_list<Playlist> expected_cache = {
        create_playlist("id1", "version1"), create_playlist("id2", "version2"),
        create_playlist("id3", "version3")};
    bool pass =
        (expected_cache == cache.playlists) && (cache.etag == "cache_etag1");
    if (pass) {
        std::cout << "test_playlist_cache_load(): PASSED\n";
    } else {
        std::cout << "test_playlist_cache_load(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void test_playlist_cache_save() {
    PlaylistCache cache(Platform::TEST);
    cache.playlists = {create_playlist("id1", "version1"),
                       create_playlist("id2", "version2"),
                       create_playlist("id3", "version3")};
    cache.etag = "cache_etag1";

    for (Playlist& pl : cache.playlists) {
        pl.was_changed = true;
    }
    cache.was_changed = true;
    cache.save();

    std::vector<std::pair<std::string, std::string>> expected_cache = {
        {"id1", "version1"}, {"id2", "version2"}, {"id3", "version3"}};
    if (assert_cache_state("cache_etag1", expected_cache)) {
        std::cout << "test_playlist_cache_save(): PASSED\n";
    } else {
        std::cout << "test_playlist_cache_save(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void test_update_to_empty_playlist_cache() {
    std::vector<Playlist> mod_playlists = {create_playlist("id1", "version1"),
                                           create_playlist("id2", "version2"),
                                           create_playlist("id3", "version3")};
    PlaylistCache cache(Platform::TEST);
    cache.update(std::move(mod_playlists), "cache_etag1");
    cache.save();

    std::vector<std::pair<std::string, std::string>> expected_cache = {
        {"id1", "version1"}, {"id2", "version2"}, {"id3", "version3"}};
    bool pass = assert_cache_state("cache_etag1", expected_cache);
    if (pass) {
        std::cout << "test_update_to_empty_playlist_cache(): PASSED\n";
    } else {
        std::cout << "test_update_to_empty_playlist_cache(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void test_update_to_non_empty_playlist_cache() {
    seed_cache("cache_etag1",
               {{"id1", "version1"}, {"id2", "version2"}, {"id3", "version3"}});

    std::vector<Playlist> mod_playlists = {
        create_playlist("id1", "version1"), create_playlist("id2", "version2"),
        create_playlist("id3", "version3"), create_playlist("id4", "version4"),
        create_playlist("id5", "version5")};
    PlaylistCache cache(Platform::TEST);
    cache.update(std::move(mod_playlists), "cache_etag2");
    cache.save();

    std::vector<std::pair<std::string, std::string>> expected_cache = {
        {"id1", "version1"},
        {"id2", "version2"},
        {"id3", "version3"},
        {"id4", "version4"},
        {"id5", "version5"}};
    bool pass = assert_cache_state("cache_etag2", expected_cache);
    if (pass) {
        std::cout << "test_update_to_non_empty_playlist_cache(): PASSED\n";
    } else {
        std::cout << "test_update_to_non_empty_playlist_cache(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void test_update_with_deletions() {
    seed_cache("cache_etag1",
               {{"id1", "version1"}, {"id2", "version2"}, {"id3", "version3"}});

    std::vector<Playlist> mod_playlists = {create_playlist("id3", "version3"),
                                           create_playlist("id4", "version4"),
                                           create_playlist("id5", "version5")};
    PlaylistCache cache(Platform::TEST);
    cache.update(std::move(mod_playlists), "cache_etag2");
    cache.save();

    std::vector<std::pair<std::string, std::string>> expected_cache = {
        {"id3", "version3"}, {"id4", "version4"}, {"id5", "version5"}};
    bool pass = assert_cache_state("cache_etag2", expected_cache);
    if (pass) {
        std::cout << "test_update_with_deletions(): PASSED\n";
    } else {
        std::cout << "test_update_with_deletions(): FAILED\n";
    }
    fs::remove_all(cache.pl_tree.root);
}

inline void run() {
    test_playlist_cache_load();
    test_playlist_cache_save();
    test_update_to_empty_playlist_cache();
    test_update_to_non_empty_playlist_cache();
    test_update_with_deletions();
}

} // namespace TestPlaylistCache
