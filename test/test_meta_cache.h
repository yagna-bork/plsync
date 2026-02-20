#include "../include/cache.h"
#include "../include/platform.h"
#include "../include/models.h"
#include "../include/config.h"
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace TestMetaCache {

std::vector<Playlist> expected_playlists = {
	{"id1", "etag1", "title1", true, 1},
	{"id2", "etag2", "title2", false, 10},
};

void test_update_empty_untracked() {
	UntrackedCache untracked(Platform::TEST);
	untracked.update(expected_playlists);

	auto cache_dir = std::filesystem::path(get_setting("cache_dir"));
	auto untracked_dir = cache_dir / "test/untracked";
	MetaCacheNode node;
	{
		std::ifstream f(untracked_dir/"HEAD.pb");
		node.ParseFromIstream(&f);
	}

	std::vector<Playlist> playlists;
	while (node.has_next()) {
		std::ifstream next(untracked_dir / node.next());
		node.ParseFromIstream(&next);
		playlists.emplace_back(
			std::string(node.entry().id()),
			std::string(node.entry().etag()),
			std::string(node.entry().title()),
			std::move(node.entry().is_private()),
			std::move(node.entry().items())
		);
	}
	
	bool success = std::equal(expected_playlists.begin(), expected_playlists.end(), 
							  playlists.begin());
	if (success) {
		std::cout << "test_update_empty_untracked(): PASSED\n";
	} else {
		std::cout << "test_update_empty_untracked(): FAILED\n";
	}
	std::filesystem::remove_all(cache_dir);
}

void run() {
	test_update_empty_untracked();
}

}
