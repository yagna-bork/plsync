#ifndef GUARD_TEST_PLAYLIST_TREE_H
#define GUARD_TEST_PLAYLIST_TREE_H
#include "../include/cache.h"
#include "../include/util.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>

namespace fs = std::filesystem;

static inline bool leaf_exists(const fs::path& leaf) {
    return fs::exists(leaf) && fs::is_regular_file(leaf);
}

namespace TestPlaylistTree {

inline void test_add_single() {
    PlaylistTree tree(Platform::TEST);
    tree.add("aa");
    fs::path leaf = tree.root / "61" / "6161";
    if (leaf_exists(leaf)) {
        std::cout << "test_add_single(): PASSED\n";
    } else {
        std::cout << "test_add_single(): FAILED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_add_double_no_conflict() {
    PlaylistTree tree(Platform::TEST);
    tree.add("aa");
    tree.add("bb");
    fs::path leaf1 = tree.root / "61" / "6161";
    fs::path leaf2 = tree.root / "62" / "6262";
    if (leaf_exists(leaf1) && leaf_exists(leaf2)) {
        std::cout << "test_add_double_no_conflict(): PASSED\n";
    } else {
        std::cout << "test_add_double_no_conflict(): FAILED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_add_double_short_conflict() {
    PlaylistTree tree(Platform::TEST);
    tree.add("aa");
    tree.add("ab");
    fs::path leaf1 = tree.root / "61" / "61" / "6161";
    fs::path leaf2 = tree.root / "61" / "62" / "6162";
    if (leaf_exists(leaf1) && leaf_exists(leaf2)) {
        std::cout << "test_add_double_short_conflict(): PASSED\n";
    } else {
        std::cout << "test_add_double_short_conflict(): FAILED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_add_double_long_conflict() {
    PlaylistTree tree(Platform::TEST);
    tree.add("abcdefghijk");
    tree.add("abcdefghijl");
    fs::path leaf1 = tree.root / "61" / "62" / "63" / "64" / "65" / "66" /
                     "67" / "68" / "69" / "6a" / "6b" /
                     "6162636465666768696a6b";
    fs::path leaf2 = tree.root / "61" / "62" / "63" / "64" / "65" / "66" /
                     "67" / "68" / "69" / "6a" / "6c" /
                     "6162636465666768696a6c";
    if (leaf_exists(leaf1) && leaf_exists(leaf2)) {
        std::cout << "test_add_double_long_conflict(): PASSED\n";
    } else {
        std::cout << "test_add_double_long_conflict(): FAILED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_search_hash_id_no_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directory(tree.root / "61");
    fs::create_directory(tree.root / "62");
    std::ofstream file1(tree.root / "61" / "61");
    std::ofstream file2(tree.root / "62" / "62");

    fs::path leaf1 = tree.search_id_hash("a");
    fs::path leaf2 = tree.search_id_hash("b");

    bool fail =
        leaf1 != tree.root / "61" / "61" || leaf2 != tree.root / "62" / "62";
    if (fail) {
        std::cout << "test_search_no_conflict(): FAILED\n";
    } else {
        std::cout << "test_search_no_conflict(): PASSED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_search_hash_id_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directories(tree.root / "61" / "61");
    fs::create_directories(tree.root / "61" / "62");
    std::ofstream file1(tree.root / "61" / "61" / "6161");
    std::ofstream file2(tree.root / "61" / "62" / "6162");

    fs::path leaf1 = tree.search_id_hash("aa");
    fs::path leaf2 = tree.search_id_hash("ab");

    bool fail = leaf1 != tree.root / "61" / "61" / "6161" ||
                leaf2 != tree.root / "61" / "62" / "6162";
    if (fail) {
        std::cout << "test_search_conflict(): FAILED\n";
    } else {
        std::cout << "test_search_conflict(): PASSED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_search_hash_id_non_existent() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directory(tree.root / "61");
    std::ofstream file1(tree.root / "61" / "61");
    fs::path p = tree.search_id_hash("b");
    if (!p.empty()) {
        std::cout << "test_search_non_existent(): FAILED\n";
    } else {
        std::cout << "test_search_non_existent(): PASSED\n";
    }
}

inline void test_search_sid_no_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directory(tree.root / "61");
    fs::create_directory(tree.root / "62");
    std::ofstream file1(tree.root / "61" / "61");
    std::ofstream file2(tree.root / "62" / "62");

    fs::path leaf1 = tree.search_sid("a");
    fs::path leaf2 = tree.search_sid("b");

    bool fail =
        leaf1 != tree.root / "61" / "61" || leaf2 != tree.root / "62" / "62";
    if (fail) {
        std::cout << "test_search_sid_no_conflict(): FAILED\n";
    } else {
        std::cout << "test_search_sid_no_conflict(): PASSED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_search_sid_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directories(tree.root / "61" / "61");
    fs::create_directories(tree.root / "61" / "62");
    std::ofstream file1(tree.root / "61" / "61" / "6161");
    std::ofstream file2(tree.root / "61" / "62" / "6162");

    fs::path leaf1 = tree.search_sid("aa");
    fs::path leaf2 = tree.search_sid("ab");

    bool fail = leaf1 != tree.root / "61" / "61" / "6161" ||
                leaf2 != tree.root / "61" / "62" / "6162";
    if (fail) {
        std::cout << "test_search_sid_conflict(): FAILED\n";
    } else {
        std::cout << "test_search_sid_conflict(): PASSED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_search_sid_non_existent() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directory(tree.root / "61");
    std::ofstream file1(tree.root / "61" / "61");
    fs::path p = tree.search_sid("b");
    if (!p.empty()) {
        std::cout << "test_search_sid_non_existent(): FAILED\n";
    } else {
        std::cout << "test_search_sid_non_existent(): PASSED\n";
    }
}

inline void test_remove_base_scenario() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directory(tree.root / "61");
    std::ofstream file(tree.root / "61" / "61");
    tree.erase("a");
    if (!fs::exists(tree.root / "61" / "61")) {
        std::cout << "test_remove_base_scenario(): PASSED\n";
    } else {
        std::cout << "test_remove_base_scenario(): FAILED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_remove_no_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directory(tree.root / "61");
    fs::create_directory(tree.root / "62");
    std::ofstream file1(tree.root / "61" / "61");
    std::ofstream file2(tree.root / "62" / "62");

    tree.erase("a");
    bool pass = !fs::exists(tree.root / "61" / "61") &&
                fs::exists(tree.root / "62" / "62");
    if (pass) {
        std::cout << "test_remove_no_conflict(): PASSED\n";
    } else {
        std::cout << "test_remove_no_conflict(): FAILED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_remove_short_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directories(tree.root / "61" / "61");
    fs::create_directories(tree.root / "61" / "62");
    std::ofstream file1(tree.root / "61" / "61" / "6161");
    std::ofstream file2(tree.root / "61" / "62" / "6162");

    tree.erase("aa");
    bool pass = !fs::exists(tree.root / "61" / "61" / "6161") &&
                !fs::exists(tree.root / "61" / "61") &&
                !fs::exists(tree.root / "61" / "62") &&
                fs::exists(tree.root / "61" / "6162");
    if (pass) {
        std::cout << "test_remove_short_conflict(): PASSED\n";
    } else {
        std::cout << "test_remove_short_conflict(): FAILED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_remove_long_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directories(tree.root / "61" / "62" / "63" / "64" / "65" / "66" /
                           "67" / "68" / "69" / "6a" / "6b");
    fs::create_directories(tree.root / "61" / "62" / "63" / "64" / "65" / "66" /
                           "67" / "68" / "69" / "6a" / "6c");
    std::ofstream file1(tree.root / "61" / "62" / "63" / "64" / "65" / "66" /
                        "67" / "68" / "69" / "6a" / "6b" /
                        "6162636465666768696a6b");
    std::ofstream file2(tree.root / "61" / "62" / "63" / "64" / "65" / "66" /
                        "67" / "68" / "69" / "6a" / "6c" /
                        "6162636465666768696a6c");

    tree.erase("abcdefghijk");
    bool pass = !fs::exists(tree.root / "61" / "62") &&
                fs::exists(tree.root / "61" / "6162636465666768696a6c");
    if (pass) {
        std::cout << "test_remove_long_conflict(): PASSED\n";
    } else {
        std::cout << "test_remove_long_conflict(): FAILED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_height_no_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directory(tree.root / "61");
    fs::create_directory(tree.root / "62");
    std::ofstream file1(tree.root / "61" / "61");
    std::ofstream file2(tree.root / "62" / "62");

    if (tree.height() != 1) {
        std::cout << "test_height_no_conflict(): FAILED\n";
    } else {
        std::cout << "test_height_no_conflict(): PASSED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_height_short_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::create_directories(tree.root / "61" / "61");
    fs::create_directories(tree.root / "61" / "62");
    std::ofstream file1(tree.root / "61" / "61" / "6161");
    std::ofstream file2(tree.root / "61" / "62" / "6162");

    if (tree.height() != 2) {
        std::cout << "test_height_short_conflict(): FAILED\n";
    } else {
        std::cout << "test_height_short_conflict(): PASSED\n";
    }
    fs::remove_all(tree.root);
}

inline void test_height_long_conflict() {
    PlaylistTree tree(Platform::TEST);
    fs::path dir1 = tree.root / "61" / "62" / "63" / "64" / "65" / "66" / "67" /
                    "68" / "69" / "6a" / "6b";
    fs::path dir2 = tree.root / "61" / "62" / "63" / "64" / "65" / "66" / "67" /
                    "68" / "69" / "6a" / "6c";
    fs::create_directories(dir1);
    fs::create_directories(dir2);
    std::ofstream file1(dir1 / "6162636465666768696a6b");
    std::ofstream file2(dir1 / "6162636465666768696a6c");

    if (tree.height() != 11) {
        std::cout << "test_height_long_conflict(): FAILED\n";
    } else {
        std::cout << "test_height_long_conflict(): PASSED\n";
    }
    fs::remove_all(tree.root);
}

inline void run() {
    test_add_single();
    test_add_double_no_conflict();
    test_add_double_short_conflict();
    test_add_double_long_conflict();

    test_search_hash_id_no_conflict();
    test_search_hash_id_conflict();
    test_search_hash_id_non_existent();

    test_search_sid_no_conflict();
    test_search_sid_conflict();
    test_search_sid_non_existent();

    test_remove_base_scenario();
    test_remove_no_conflict();
    test_remove_short_conflict();
    test_remove_long_conflict();

    test_height_no_conflict();
    test_height_short_conflict();
    test_height_long_conflict();
}

} // namespace TestPlaylistTree
#endif
