#ifndef GUARD_TEST_SONGS_DIFF_H 
#define GUARD_TEST_SONGS_DIFF_H 
#include "../include/cache.h"
#include <iostream>

namespace TestPlaylistDiff {

void test_subtract_song_hashes_added_remove() {
	std::unordered_map<size_t, int> lhs, rhs;
	lhs[1234] = 2;
	lhs[4321] = 1;
	rhs[1234] = 1;
	rhs[4321] = 2;
	PlaylistDiff diff = lhs - rhs;

	bool failed = diff.added.size() != 1 || 
				  diff.added.count(1234) != 1 || 
				  diff.added[1234] != 1 ||
				  diff.removed.size() != 1 ||
				  diff.removed.count(4321) != 1 ||
				  diff.removed[4321] != 1;
	if (failed) {
		std::cout << "test_subtract_song_hashes_added_remove(): FAILED\n";
	} else {
		std::cout << "test_subtract_song_hashes_added_remove(): PASSED\n";
	}
}

void test_subtract_song_hashes_equal() {
	std::unordered_map<size_t, int> lhs;
	lhs[1234] = 2;
	lhs[4321] = 1;
	PlaylistDiff diff = lhs - lhs;

	if (!diff.added.empty() || !diff.removed.empty()) {
		std::cout << "test_subtract_song_hashes_equal(): FAILED\n";
	} else {
		std::cout << "test_subtract_song_hashes_equal(): PASSED\n";
	}
}

void test_subtract_song_hashes_only_added() {
	std::unordered_map<size_t, int> lhs, rhs;
	lhs[1234] = 2;
	lhs[4321] = 1;
	PlaylistDiff diff = lhs - rhs;

	bool failed = diff.added.size() != 2 || 
				  !diff.added.count(1234) || 
				  diff.added[1234] != 2 ||
				  !diff.added.count(4321) ||
				  diff.added[4321] != 1 ||
				  !diff.removed.empty();
	if (failed) {
		std::cout << "test_subtract_song_hashes_only_added(): FAILED\n";
	} else {
		std::cout << "test_subtract_song_hashes_only_added(): PASSED\n";
	}
}

void test_subtract_song_hashes_only_removed() {
	std::unordered_map<size_t, int> lhs, rhs;
	rhs[1234] = 2;
	rhs[4321] = 1;
	PlaylistDiff diff = lhs - rhs;

	bool failed = diff.removed.size() != 2 || 
				  !diff.removed.count(1234) || 
				  diff.removed[1234] != 2 ||
				  !diff.removed.count(4321) ||
				  diff.removed[4321] != 1 ||
				  !diff.added.empty();
	if (failed) {
		std::cout << "test_subtract_song_hashes_only_removed(): FAILED\n";
	} else {
		std::cout << "test_subtract_song_hashes_only_removed(): PASSED\n";
	}
}

void test_addition_playlist_diff() {
	PlaylistDiff diff1 = { /*added=*/{{1, 1}, {2, 1}}, /*removed=*/{{11, 1}, {12, 2}} };
	PlaylistDiff diff2 = { /*added=*/{{2, 2}, {3, 1}}, /*removed=*/{{12, 1}, {13, 1}} };
	PlaylistDiff diff3 = diff1;
	diff3 += diff2;
	
	PlaylistDiff expected = { /*added=*/{{1, 1}, {2, 2}, {3, 1}}, /*removed=*/{{11, 1}, {12, 2}, {13, 1}} };
	if (diff3 != expected) {
		std::cout << "test_addition_song_diffs(): FAILED\n";
	} else {
		std::cout << "test_addition_song_diffs(): PASSED\n";
	}
}

void test_addition_playlist_diff_ambiguous_left_bias() {
	PlaylistDiff diff1 = { /*added=*/{{1, 1}}, /*removed=*/{} };
	PlaylistDiff diff2 = { /*added=*/{}, /*removed=*/{{1, 1}} };
	PlaylistDiff diff3 = diff1;
	diff3 += diff2;
	
	PlaylistDiff expected = { /*added=*/{{1, 1}}, /*removed=*/{} };
	if (diff3 != expected) {
		std::cout << "test_addition_playlist_diff_ambiguous_left_bias(): FAILED\n";
	} else {
		std::cout << "test_addition_playlist_diff_ambiguous_left_bias(): PASSED\n";
	}
}

void test_addition_playlist_diff_ambiguous_larger_magnitude() {
	PlaylistDiff diff1 = { /*added=*/{{1, 1}}, /*removed=*/{} };
	PlaylistDiff diff2 = { /*added=*/{}, /*removed=*/{{1, 3}} };
	PlaylistDiff diff3 = diff1;
	diff3 += diff2;
	
	PlaylistDiff expected = { /*added=*/{}, /*removed=*/{{1, 3}} };
	if (diff3 != expected) {
		std::cout << "test_addition_playlist_diff_ambiguous_larger_magnitude(): FAILED\n";
	} else {
		std::cout << "test_addition_playlist_diff_ambiguous_larger_magnitude(): PASSED\n";
	}
}

void test_subtract_playlist_diff() {
	PlaylistDiff lhs = { /*added=*/{{1, 3}, {2, 2}}, /*removed=*/{{3, 3}} };
	PlaylistDiff rhs = { /*added=*/{{1, 2}, {2, 2}, {3, 2}}, /*removed=*/{} };
	PlaylistDiff res = lhs - rhs;

	PlaylistDiff expected = { /*added=*/{{1, 1}}, /*removed=*/{{3, 5}} };
	if (res != expected) {
		std::cout << "test_subtract_playlist_diff(): FAILED\n";
	} else {
		std::cout << "test_subtract_playlist_diff(): PASSED\n";
	}
}

void run() {
	test_subtract_song_hashes_added_remove();
	test_subtract_song_hashes_equal();
	test_subtract_song_hashes_only_added();
	test_subtract_song_hashes_only_removed();
	test_addition_playlist_diff();
	test_addition_playlist_diff_ambiguous_left_bias();
	test_addition_playlist_diff_ambiguous_larger_magnitude();
	test_subtract_playlist_diff();
}

}
#endif
