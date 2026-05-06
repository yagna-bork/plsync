#ifndef GUARD_TEST_SONGS_DIFF_H
#define GUARD_TEST_SONGS_DIFF_H
#include "../include/cache.h"
#include <iostream>

namespace TestPlaylistDiff {

static const Song song1 = {{"Artic Monkeys"}, "505"};
static const Song song2 = {{"girl in red"}, "girls"};
static const Song song3 = {{"Clairo"}, "pretty girl"};
static const Song song4 = {{"The Rare Occasions"}, "Notion"};
static const Song song5 = {{"Surf Curse"}, "Freaks"};
static const Song song6 = {{"bedroom"}, "in my head"};

static void test_subtract_song_hashes_added_remove() {
    SongCounts lhs, rhs;
    lhs[song1] = 2;
    lhs[song2] = 1;
    rhs[song1] = 1;
    rhs[song2] = 2;
    PlaylistDiff diff = lhs - rhs;

    bool failed = diff.added.size() != 1 || diff.added.count(song1) != 1 ||
                  diff.added[song1] != 1 || diff.removed.size() != 1 ||
                  diff.removed.count(song2) != 1 || diff.removed[song2] != 1;
    if (failed) {
        std::cout << "test_subtract_song_hashes_added_remove(): FAILED\n";
    } else {
        std::cout << "test_subtract_song_hashes_added_remove(): PASSED\n";
    }
}

static void test_subtract_song_hashes_equal() {
    SongCounts lhs;
    lhs[song1] = 2;
    lhs[song2] = 1;
    PlaylistDiff diff = lhs - lhs;

    if (!diff.added.empty() || !diff.removed.empty()) {
        std::cout << "test_subtract_song_hashes_equal(): FAILED\n";
    } else {
        std::cout << "test_subtract_song_hashes_equal(): PASSED\n";
    }
}

static void test_subtract_song_hashes_only_added() {
    SongCounts lhs, rhs;
    lhs[song1] = 2;
    lhs[song2] = 1;
    PlaylistDiff diff = lhs - rhs;

    bool failed = diff.added.size() != 2 || !diff.added.count(song1) ||
                  diff.added[song1] != 2 || !diff.added.count(song2) ||
                  diff.added[song2] != 1 || !diff.removed.empty();
    if (failed) {
        std::cout << "test_subtract_song_hashes_only_added(): FAILED\n";
    } else {
        std::cout << "test_subtract_song_hashes_only_added(): PASSED\n";
    }
}

static void test_subtract_song_hashes_only_removed() {
    SongCounts lhs, rhs;
    rhs[song1] = 2;
    rhs[song2] = 1;
    PlaylistDiff diff = lhs - rhs;

    bool failed = diff.removed.size() != 2 || !diff.removed.count(song1) ||
                  diff.removed[song1] != 2 || !diff.removed.count(song2) ||
                  diff.removed[song2] != 1 || !diff.added.empty();
    if (failed) {
        std::cout << "test_subtract_song_hashes_only_removed(): FAILED\n";
    } else {
        std::cout << "test_subtract_song_hashes_only_removed(): PASSED\n";
    }
}

static void test_addition_playlist_diff() {
    PlaylistDiff diff1 = {/*added=*/{{song1, 1}, {song2, 1}},
                          /*removed=*/{{song4, 1}, {song5, 2}}};
    PlaylistDiff diff2 = {/*added=*/{{song2, 2}, {song3, 1}},
                          /*removed=*/{{song5, 1}, {song6, 1}}};
    PlaylistDiff diff3 = diff1;
    diff3 += diff2;

    PlaylistDiff expected = {/*added=*/{{song1, 1}, {song2, 2}, {song3, 1}},
                             /*removed=*/{{song4, 1}, {song5, 2}, {song6, 1}}};
    if (diff3 == expected) {
        std::cout << "test_addition_song_diffs(): PASSED\n";
    } else {
        std::cout << "test_addition_song_diffs(): FAILED\n";
    }
}

static void test_addition_playlist_diff_ambiguous_left_bias() {
    PlaylistDiff diff1 = {/*added=*/{{song1, 1}}, /*removed=*/{}};
    PlaylistDiff diff2 = {/*added=*/{}, /*removed=*/{{song1, 1}}};
    PlaylistDiff diff3 = diff1;
    diff3 += diff2;

    PlaylistDiff expected = {/*added=*/{{song1, 1}}, /*removed=*/{}};
    if (diff3 == expected) {
        std::cout
            << "test_addition_playlist_diff_ambiguous_left_bias(): PASSED\n";
    } else {
        std::cout
            << "test_addition_playlist_diff_ambiguous_left_bias(): FAILED\n";
    }
}

static void test_addition_playlist_diff_ambiguous_larger_magnitude() {
    PlaylistDiff diff1 = {/*added=*/{{song1, 1}}, /*removed=*/{}};
    PlaylistDiff diff2 = {/*added=*/{}, /*removed=*/{{song1, 3}}};
    PlaylistDiff diff3 = diff1;
    diff3 += diff2;

    PlaylistDiff expected = {/*added=*/{}, /*removed=*/{{song1, 3}}};
    if (diff3 == expected) {
        std::cout << "test_addition_playlist_diff_ambiguous_larger_magnitude():"
                     " PASSED\n";
    } else {
        std::cout << "test_addition_playlist_diff_ambiguous_larger_magnitude():"
                     " FAILED\n";
    }
}

static void test_subtract_playlist_diff() {
    PlaylistDiff lhs = {/*added=*/{{song1, 3}, {song2, 2}},
                        /*removed=*/{{song3, 3}}};
    PlaylistDiff rhs = {/*added=*/{{song1, 2}, {song2, 2}, {song3, 2}},
                        /*removed=*/{}};
    PlaylistDiff res = lhs - rhs;

    PlaylistDiff expected = {/*added=*/{{song1, 1}}, /*removed=*/{{song3, 5}}};
    if (res == expected) {
        std::cout << "test_subtract_playlist_diff(): PASSED\n";
    } else {
        std::cout << "test_subtract_playlist_diff(): FAILED\n";
    }
}

inline void run() {
    test_subtract_song_hashes_added_remove();
    test_subtract_song_hashes_equal();
    test_subtract_song_hashes_only_added();
    test_subtract_song_hashes_only_removed();
    test_addition_playlist_diff();
    test_addition_playlist_diff_ambiguous_left_bias();
    test_addition_playlist_diff_ambiguous_larger_magnitude();
    test_subtract_playlist_diff();
}

} // namespace TestPlaylistDiff
#endif
