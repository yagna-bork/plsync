#include "../include/util.h"
#include <iostream>
#include <string>
#include <vector>

namespace TestUtil {
void test_bin_to_hex() {
    std::vector<std::string> expected = {"00", "01", "02", "03", "04", "05",
                                         "06", "07", "08", "09", "0a", "0b",
                                         "0c", "0d", "0e", "0f"};
    std::vector<std::string> actual;
    for (std::size_t i = 0; i != 16; i++) {
        actual.emplace_back(bin_to_hex(std::string(1, i)));
    }
    if (actual == expected) {
        std::cout << "test_bin_to_hex(): PASSED\n";
    } else {
        std::cout << "test_bin_to_hex(): FAILED\n";
    }
}

void test_sha1() {
    std::string actual = bin_to_hex(sha1("abcdefghijklmnopqrstuvwxyz"));
    std::string expected = "32d10c7b8cf96570ca04ce37f2a19d84240d3a89";
    if (actual != expected) {
        std::cout << "test_sha1(): FAILED\n";
    } else {
        std::cout << "test_sha1(): PASSED\n";
    }
}

void test_sha256() {
    std::string actual = bin_to_hex(sha256("abcdefghijklmnopqrstuvwxyz"));
    std::string expected =
        "71c480df93d6ae2f1efad1447c66c9525e316218cf51fc8d9ed832f2daf18b73";
    if (actual != expected) {
        std::cout << "test_sha256(): FAILED\n";
    } else {
        std::cout << "test_sha256(): PASSED\n";
    }
}

void run() {
    test_bin_to_hex();
    test_sha1();
    test_sha256();
}
} // namespace TestUtil
