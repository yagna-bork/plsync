#include "test_playlist_cache.h"
#include "test_playlist_diff.h"
#include "test_playlist_tree.h"
#include "test_token_store.h"
#include "test_util.h"
#include <iostream>

int main() {
    std::cout << "Token storage tests\n";
    std::cout << "-------------------\n";
    TestTokenStorage::run();

    std::cout << "\n\nPlaylist cache tests\n";
    std::cout << "--------------------\n";
    TestPlaylistCache::run();

    std::cout << "\n\nUtil tests\n";
    std::cout << "----------\n";
    TestUtil::run();

    std::cout << "\n\nSongs diff tests\n";
    std::cout << "----------------\n";
    TestPlaylistDiff::run();

    std::cout << "\n\nPlaylist tree tests\n";
    std::cout << "-------------------\n";
    TestPlaylistTree::run();
    return 0;
}
