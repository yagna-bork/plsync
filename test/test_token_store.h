#ifndef GUARD_TEST_TOKEN_STORE_H
#define GUARD_TEST_TOKEN_STORE_H
#include "../include/platform.h"
#include "../include/util.h"
#include <iostream>
#include <iterator>
#include <libcred.hpp>
#include <stdexcept>
#include <string>

namespace TestTokenStorage {

const std::string pwd = "test-token";
const std::string TOKEN = "test-token";

inline void test_save_and_get_access_tkn() {
    std::time_t now = std::time(nullptr);
    std::string tkn;
    auto curl = get_curl();
    try {
        save_access_tkn(Platform::TEST, TOKEN, 1000);
        get_or_fetch_access_tkn(Platform::TEST, curl, tkn);
    } catch (const TokenStorageAccessError& e) {
        std::cout << "test_save_and_get_access_tkn(): FAILED\n";
        return;
    }
    if (tkn != TOKEN) {
        std::cout << "test_save_and_get_access_tkn(): FAILED\n";
    } else {
        std::cout << "test_save_and_get_access_tkn(): PASSED\n";
    }
}

inline void test_save_and_get_refresh_tkn() {
    std::string tkn;
    try {
        save_refresh_tkn(Platform::TEST, TOKEN);
        get_refresh_tkn(Platform::TEST, tkn);
    } catch (const TokenStorageAccessError& e) {
        std::cout << "test_save_and_get_refresh_tkn(): FAILED\n";
        return;
    }
    if (tkn != TOKEN) {
        std::cout << "test_save_and_get_refresh_tkn(): FAILED\n";
    } else {
        std::cout << "test_save_and_get_refresh_tkn(): PASSED\n";
    }
}

inline void run() {
    test_save_and_get_access_tkn();
    test_save_and_get_refresh_tkn();
}

} // namespace TestTokenStorage
#endif
