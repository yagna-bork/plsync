/*
#include "../include/cache.h"
#include "../include/platform.h"
#include "../include/models.h"
#include "../include/config.h"
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <ostream>
#include <iterator>

std::ostream &operator<<(std::ostream &os, const Playlist &pl) {
	return os << "id = " << pl.id << " etag = " << pl.etag 
	   << " title = " << pl.title << " is_private = " << pl.is_private 
	   << " items = " << pl.items << " short_id = " << pl.short_id;
}

inline bool operator==(const Playlist &lhs, const Playlist &rhs) {
	return lhs.id == rhs.id && 
		   lhs.etag == rhs.etag && 
		   lhs.title == rhs.title &&
		   lhs.is_private == rhs.is_private &&
		   lhs.items == rhs.items &&
		   lhs.short_id == rhs.short_id;
}

inline bool operator!=(const Playlist &lhs, const Playlist &rhs) {
	return !(lhs == rhs);
}

namespace TestPlaylistCache {


void print_playlists(const std::vector<Playlist> &playlists, const std::string &name) {
	std::ostream_iterator<Playlist> playlist_out(std::cout, "\n");
	std::cout << name << '\n';
	std::copy(playlists.begin(), playlists.end(), playlist_out);
}

std::vector<Playlist> parse_playlists_from_cache(std::filesystem::path cache_dir) {
	PlaylistCacheNode node;
	{
		std::ifstream f(cache_dir/"HEAD.pb");
		node.ParseFromIstream(&f);
	}

	std::vector<Playlist> playlists;
	while (!node.next().empty()) {
		std::ifstream next(cache_dir / node.next());
		node.ParseFromIstream(&next);
		playlists.emplace_back(
			std::string(node.entry().id()),
			std::string(node.entry().etag()),
			std::string(node.entry().title()),
			std::move(node.entry().is_private()),
			std::move(node.entry().items())
		);
	}
	return playlists;
}

// lowercase hex char to a binary char
unsigned char hex_to_bin(char c) {
	if ('0' <= c && c <= '9') {
		return c - '0';
	}
	return c - 'a' + 10;
}

// lowercase hex string to a raw binary string
std::string hex_to_bin(const std::string &hex_str) {
	std::size_t n = hex_str.size();
	std::string res;
	res.reserve(n / 2);
	unsigned char byte;
	for (std::size_t i = 0; i != n; i += 2) {
		byte = (hex_to_bin(hex_str[i]) << 4) + hex_to_bin(hex_str[i+1]);
		res.push_back(byte);
	}
	return res;
}

void test_update_empty_untracked() {
	std::vector<Playlist> expected_playlists = {
		{"id1", "etag1", "title1", true, 1},
		{"id2", "etag2", "title2", false, 10},
	};
	UntrackedCache untracked(Platform::TEST);
	untracked.update(expected_playlists);

	auto cache_dir = std::filesystem::path(get_setting("cache_dir"));
	auto untracked_dir = cache_dir / "test/untracked";
	std::vector<Playlist> playlists = parse_playlists_from_cache(untracked_dir);

	if (playlists == expected_playlists) {
		std::cout << "test_update_empty_untracked(): PASSED\n";
	} else {
		std::cout << "test_update_empty_untracked(): FAILED\n";
	}
	std::filesystem::remove_all(cache_dir);
}

void test_update_untracked_unchanged() {
	std::vector<Playlist> expected_playlists = {{"id1", "etag1", "title1", true, 1}};
	UntrackedCache untracked(Platform::TEST);
	untracked.update(expected_playlists);
	untracked.update(expected_playlists);

	auto cache_dir = std::filesystem::path(get_setting("cache_dir"));
	auto untracked_dir = cache_dir / "test/untracked";
	auto playlists = parse_playlists_from_cache(untracked_dir);

	if (playlists == expected_playlists) {
		std::cout << "test_update_untracked_unchanged(): PASSED\n";
	} else {
		std::cout << "test_update_untracked_unchanged(): FAILED\n";
	}
	std::filesystem::remove_all(cache_dir);
}

void test_update_untracked_changed() {
	std::vector<Playlist> expected_playlists = {{"id1", "etag1", "title1", true, 1}};
	UntrackedCache untracked(Platform::TEST);
	untracked.update(expected_playlists);
	
	expected_playlists[0].etag = "etag2";
	expected_playlists[0].title = "title2";
	expected_playlists[0].is_private = false;
	expected_playlists[0].items = 2;
	untracked.update(expected_playlists);

	auto cache_dir = std::filesystem::path(get_setting("cache_dir"));
	auto untracked_dir = cache_dir / "test/untracked";
	auto playlists = parse_playlists_from_cache(untracked_dir);

	if (playlists == expected_playlists) {
		std::cout << "test_update_untracked_changed(): PASSED\n";
	} else {
		std::cout << "test_update_untracked_changed(): FAILED\n";
	}
	std::filesystem::remove_all(cache_dir);
}

void test_update_untracked_delete() {
	std::vector<Playlist> expected_playlists = {{"id1", "etag1", "title1", true, 1}};
	UntrackedCache untracked(Platform::TEST);
	untracked.update(expected_playlists);
	
	expected_playlists.pop_back();
	untracked.update(expected_playlists);
	
	auto cache_dir = std::filesystem::path(get_setting("cache_dir"));
	auto untracked_dir = cache_dir / "test/untracked";
	auto playlists = parse_playlists_from_cache(untracked_dir);

	if (playlists == expected_playlists) {
		std::cout << "test_update_untracked_delete(): PASSED\n";
	} else {
		std::cout << "test_update_untracked_delete(): FAILED\n";
	}
	std::filesystem::remove_all(cache_dir);
}

void test_get_playlists() {
	auto cache_dir = std::filesystem::path(get_setting("cache_dir"));
	std::vector<Playlist> expected_playlists = {
		{"id1", "etag1", "title1", true, 1, hex_to_bin("f3")},
		{"id2", "etag2", "title2", false, 10, hex_to_bin("b4")},
		{"id3", "etag3", "title3", true, 20, hex_to_bin("04")}
	};
	UntrackedCache untracked(Platform::TEST);
	untracked.update(expected_playlists);
	std::vector<Playlist> playlists = untracked.get_playlists();
	print_playlists(playlists, "actual");

	if (playlists == expected_playlists) {
		std::cout << "test_get_playlists(): PASSED\n";
	} else {
		std::cout << "test_get_playlists(): FAILED\n";
	}
	std::filesystem::remove_all(cache_dir);
}

void test_get_playlists_sorted() {
	auto cache_dir = std::filesystem::path(get_setting("cache_dir"));
	std::vector<Playlist> unsorted_playlists = {
		{"id3", "etag3", "title3", true, 20},
		{"id2", "etag2", "title2", false, 10},
		{"id1", "etag1", "title1", true, 1}
	};
	UntrackedCache untracked(Platform::TEST);
	untracked.update(unsorted_playlists);
	std::vector<Playlist> playlists = untracked.get_playlists_sorted();

	std::vector<Playlist> sorted_playlists = {
		{"id1", "etag1", "title1", true, 1, hex_to_bin("f3")},
		{"id2", "etag2", "title2", false, 10, hex_to_bin("b4")},
		{"id3", "etag3", "title3", true, 20, hex_to_bin("04")}
	};
	if (playlists == sorted_playlists) {
		std::cout << "test_get_sorted_playlists(): PASSED\n";
	} else {
		std::cout << "test_get_sorted_playlists(): FAILED\n";
	}
	std::filesystem::remove_all(cache_dir);
}

void run() {
	//test_update_empty_untracked();
	//test_update_untracked_unchanged();
	//test_update_untracked_changed();
	//test_update_untracked_delete();
	test_get_playlists();
	//test_get_playlists_sorted();
}

}
*/
