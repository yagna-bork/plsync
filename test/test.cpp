//#include "test_token_store.h" TODO
#include "test_api.h"
// #include "test_playlist_cache.h" TODO
#include "test_playlist_diff.h"
#include "test_util.h"
#include "../include/platform.h"
#include <iostream>
#include <string>

int main() {
	std::cout << "Token storage tests\n";
	std::cout << "-------------------\n";
	// TestTokenStorage::run();

	std::cout << "\n\nYoutube API tests\n";
	std::cout << "-----------------\n";
	// TODO TestAPI::run();

	/*
	std::cout << "\n\nPlaylist cache tests\n";
	std::cout << "-----------------\n";
	TestPlaylistCache::run();
	*/
	
	std::cout << "\n\nUtil tests\n";
	std::cout << "----------\n";
	TestUtil::run();

	std::cout << "\n\nSongs diff tests\n";
	std::cout << "----------\n";
	TestPlaylistDiff::run();
	return 0;
}
