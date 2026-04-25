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

fs::path get_ensure_root(Platform plat);

namespace TestPlaylistTree {

bool leaf_exists(const fs::path& leaf) {
    return fs::exists(leaf) && fs::is_regular_file(leaf);
}

void test_add_single() {
    playlist_tree_add("aa", Platform::TEST);
    fs::path root = get_ensure_root(Platform::TEST);
    fs::path leaf = root / "61" / "6161";
    if (leaf_exists(leaf)) {
        std::cout << "test_add_single(): PASSED\n";
    } else {
        std::cout << "test_add_single(): FAILED\n";
    }
    fs::remove_all(root);
}

void test_add_double_no_conflict() {
    playlist_tree_add("aa", Platform::TEST);
    playlist_tree_add("bb", Platform::TEST);
    fs::path root = get_ensure_root(Platform::TEST);
    fs::path leaf1 = root / "61" / "6161";
    fs::path leaf2 = root / "62" / "6262";
    if (leaf_exists(leaf1) && leaf_exists(leaf2)) {
        std::cout << "test_add_double_no_conflict(): PASSED\n";
    } else {
        std::cout << "test_add_double_no_conflict(): FAILED\n";
    }
    fs::remove_all(root);
}

void test_add_double_short_conflict() {
    playlist_tree_add("aa", Platform::TEST);
    playlist_tree_add("ab", Platform::TEST);
    fs::path root = get_ensure_root(Platform::TEST);
    fs::path leaf1 = root / "61" / "61" / "6161";
    fs::path leaf2 = root / "61" / "62" / "6162";
    if (leaf_exists(leaf1) && leaf_exists(leaf2)) {
        std::cout << "test_add_double_short_conflict(): PASSED\n";
    } else {
        std::cout << "test_add_double_short_conflict(): FAILED\n";
    }
    fs::remove_all(root);
}

void test_add_double_long_conflict() {
    playlist_tree_add("abcdefghijk", Platform::TEST);
    playlist_tree_add("abcdefghijl", Platform::TEST);
    fs::path root = get_ensure_root(Platform::TEST);
    fs::path leaf1 = root / "61" / "62" / "63" / "64" / "65" / "66" / "67" /
                     "68" / "69" / "6a" / "6b" / "6162636465666768696a6b";
    fs::path leaf2 = root / "61" / "62" / "63" / "64" / "65" / "66" / "67" /
                     "68" / "69" / "6a" / "6c" / "6162636465666768696a6c";
    if (leaf_exists(leaf1) && leaf_exists(leaf2)) {
        std::cout << "test_add_double_long_conflict(): PASSED\n";
    } else {
        std::cout << "test_add_double_long_conflict(): FAILED\n";
    }
    fs::remove_all(root);
}

void test_search_hash_id_no_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directory(root / "61");
    fs::create_directory(root / "62");
    std::ofstream file1(root / "61" / "61");
    std::ofstream file2(root / "62" / "62");

    fs::path leaf1, leaf2;
    std::string sid1, sid2;
    playlist_tree_path_sid_from_id_hash("a", Platform::TEST, leaf1, sid1);
    playlist_tree_path_sid_from_id_hash("b", Platform::TEST, leaf2, sid2);

    bool fail = leaf1 != root / "61" / "61" || sid1 != "a" ||
                leaf2 != root / "62" / "62" || sid2 != "b";
    if (fail) {
        std::cout << "test_search_no_conflict(): FAILED\n";
    } else {
        std::cout << "test_search_no_conflict(): PASSED\n";
    }
    fs::remove_all(root);
}

void test_search_hash_id_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directories(root / "61" / "61");
    fs::create_directories(root / "61" / "62");
    std::ofstream file1(root / "61" / "61" / "6161");
    std::ofstream file2(root / "61" / "62" / "6162");

    fs::path leaf1, leaf2;
    std::string sid1, sid2;
    playlist_tree_path_sid_from_id_hash("aa", Platform::TEST, leaf1, sid1);
    playlist_tree_path_sid_from_id_hash("ab", Platform::TEST, leaf2, sid2);

    bool fail = leaf1 != root / "61" / "61" / "6161" || sid1 != "aa" ||
                leaf2 != root / "61" / "62" / "6162" || sid2 != "ab";
    if (fail) {
        std::cout << "test_search_conflict(): FAILED\n";
    } else {
        std::cout << "test_search_conflict(): PASSED\n";
    }
    fs::remove_all(root);
}

void test_search_hash_id_non_existent() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directory(root / "61");
    std::ofstream file1(root / "61" / "61");
    fs::path p = playlist_tree_path_from_id_hash("b", Platform::TEST);
    if (!p.empty()) {
        std::cout << "test_search_non_existent(): FAILED\n";
    } else {
        std::cout << "test_search_non_existent(): PASSED\n";
    }
}

void test_search_sid_no_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directory(root / "61");
    fs::create_directory(root / "62");
    std::ofstream file1(root / "61" / "61");
    std::ofstream file2(root / "62" / "62");

    fs::path leaf1 = playlist_tree_path_from_sid("a", Platform::TEST);
    fs::path leaf2 = playlist_tree_path_from_sid("b", Platform::TEST);

    bool fail = leaf1 != root / "61" / "61" || leaf2 != root / "62" / "62";
    if (fail) {
        std::cout << "test_search_sid_no_conflict(): FAILED\n";
    } else {
        std::cout << "test_search_sid_no_conflict(): PASSED\n";
    }
    fs::remove_all(root);
}

void test_search_sid_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directories(root / "61" / "61");
    fs::create_directories(root / "61" / "62");
    std::ofstream file1(root / "61" / "61" / "6161");
    std::ofstream file2(root / "61" / "62" / "6162");

    fs::path leaf1 = playlist_tree_path_from_sid("aa", Platform::TEST);
    fs::path leaf2 = playlist_tree_path_from_sid("ab", Platform::TEST);

    bool fail = leaf1 != root / "61" / "61" / "6161" ||
                leaf2 != root / "61" / "62" / "6162";
    if (fail) {
        std::cout << "test_search_sid_conflict(): FAILED\n";
    } else {
        std::cout << "test_search_sid_conflict(): PASSED\n";
    }
    fs::remove_all(root);
}

void test_search_sid_non_existent() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directory(root / "61");
    std::ofstream file1(root / "61" / "61");
    fs::path p = playlist_tree_path_from_sid("b", Platform::TEST);
    if (!p.empty()) {
        std::cout << "test_search_sid_non_existent(): FAILED\n";
    } else {
        std::cout << "test_search_sid_non_existent(): PASSED\n";
    }
}

void test_remove_base_scenario() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directory(root / "61");
    std::ofstream file(root / "61" / "61");
    playlist_tree_remove("a", Platform::TEST);
    if (!fs::exists(root / "61" / "61")) {
        std::cout << "test_remove_base_scenario(): PASSED\n";
    } else {
        std::cout << "test_remove_base_scenario(): FAILED\n";
    }
    fs::remove_all(root);
}

void test_remove_no_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directory(root / "61");
    fs::create_directory(root / "62");
    std::ofstream file1(root / "61" / "61");
    std::ofstream file2(root / "62" / "62");

    playlist_tree_remove("a", Platform::TEST);
    bool pass =
        !fs::exists(root / "61" / "61") && fs::exists(root / "62" / "62");
    if (pass) {
        std::cout << "test_remove_no_conflict(): PASSED\n";
    } else {
        std::cout << "test_remove_no_conflict(): FAILED\n";
    }
    fs::remove_all(root);
}

void test_remove_short_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directories(root / "61" / "61");
    fs::create_directories(root / "61" / "62");
    std::ofstream file1(root / "61" / "61" / "6161");
    std::ofstream file2(root / "61" / "62" / "6162");

    playlist_tree_remove("aa", Platform::TEST);
    bool pass = !fs::exists(root / "61" / "61" / "6161") &&
                !fs::exists(root / "61" / "61") &&
                !fs::exists(root / "61" / "62") &&
                fs::exists(root / "61" / "6162");
    if (pass) {
        std::cout << "test_remove_short_conflict(): PASSED\n";
    } else {
        std::cout << "test_remove_short_conflict(): FAILED\n";
    }
    fs::remove_all(root);
}

void test_remove_long_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directories(root / "61" / "62" / "63" / "64" / "65" / "66" /
                           "67" / "68" / "69" / "6a" / "6b");
    fs::create_directories(root / "61" / "62" / "63" / "64" / "65" / "66" /
                           "67" / "68" / "69" / "6a" / "6c");
    std::ofstream file1(root / "61" / "62" / "63" / "64" / "65" / "66" / "67" /
                        "68" / "69" / "6a" / "6b" / "6162636465666768696a6b");
    std::ofstream file2(root / "61" / "62" / "63" / "64" / "65" / "66" / "67" /
                        "68" / "69" / "6a" / "6c" / "6162636465666768696a6c");

    playlist_tree_remove("abcdefghijk", Platform::TEST);
    bool pass = !fs::exists(root / "61" / "62") &&
                fs::exists(root / "61" / "6162636465666768696a6c");
    if (pass) {
        std::cout << "test_remove_long_conflict(): PASSED\n";
    } else {
        std::cout << "test_remove_long_conflict(): FAILED\n";
    }
    fs::remove_all(root);
}

void test_height_no_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directory(root / "61");
    fs::create_directory(root / "62");
    std::ofstream file1(root / "61" / "61");
    std::ofstream file2(root / "62" / "62");

    if (playlist_tree_height(Platform::TEST) != 1) {
        std::cout << "test_height_no_conflict(): FAILED\n";
    } else {
        std::cout << "test_height_no_conflict(): PASSED\n";
    }
    fs::remove_all(root);
}

void test_height_short_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::create_directories(root / "61" / "61");
    fs::create_directories(root / "61" / "62");
    std::ofstream file1(root / "61" / "61" / "6161");
    std::ofstream file2(root / "61" / "62" / "6162");

    if (playlist_tree_height(Platform::TEST) != 2) {
        std::cout << "test_height_short_conflict(): FAILED\n";
    } else {
        std::cout << "test_height_short_conflict(): PASSED\n";
    }
    fs::remove_all(root);
}

void test_height_long_conflict() {
    fs::path root = get_ensure_root(Platform::TEST);
    fs::path dir1 = root / "61" / "62" / "63" / "64" / "65" / "66" / "67" /
                    "68" / "69" / "6a" / "6b";
    fs::path dir2 = root / "61" / "62" / "63" / "64" / "65" / "66" / "67" /
                    "68" / "69" / "6a" / "6c";
    fs::create_directories(dir1);
    fs::create_directories(dir2);
    std::ofstream file1(dir1 / "6162636465666768696a6b");
    std::ofstream file2(dir1 / "6162636465666768696a6c");

    if (playlist_tree_height(Platform::TEST) != 11) {
        std::cout << "test_height_long_conflict(): FAILED\n";
    } else {
        std::cout << "test_height_long_conflict(): PASSED\n";
    }
    fs::remove_all(root);
}

void run() {
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
