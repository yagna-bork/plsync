#ifndef GUARD_TEST_TOKEN_STORE_H
#define GUARD_TEST_TOKEN_STORE_H
#include "../include/token_store.h"
#include "../include/platform.h"
#include "../include/util.h"
#include <string>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <libcred.hpp>

static const std::string TOKEN = "test-token";

namespace TestTokenStorage {

void test_save_and_fetch_access_tkn(Platform platform) {
	std::time_t now = std::time(nullptr);
	if (!save_access_tkn(platform, TOKEN, 1000)) {
		std::cout << "test_save_and_fetch_access_tkn(): FAILED\n";
		return;
	}
	std::string tkn;
	std::time_t expiry;
	if (!fetch_access_tkn(platform, tkn, expiry)) {
		std::cout << "test_save_and_fetch_access_tkn(): FAILED\n";
		return;
	}
	if (tkn != TOKEN || expiry != now+1000) {
		std::cout << "test_save_and_fetch_access_tkn(): FAILED\n";
	} else {
		std::cout << "test_save_and_fetch_access_tkn(): PASSED\n";
	}
}

void test_save_and_fetch_refresh_tkn(Platform platform) {
	if (!save_refresh_tkn(platform, TOKEN)) {
		std::cout << "test_save_and_fetch_refresh_tkn(): FAILED\n";
		return;
	}
	std::string tkn;
	if (!fetch_refresh_tkn(platform, tkn)) {
		std::cout << "test_save_and_fetch_refresh_tkn(): FAILED\n";
		return;
	}
	if (tkn != TOKEN) {
		std::cout << "test_save_and_fetch_refresh_tkn(): FAILED\n";
	} else {
		std::cout << "test_save_and_fetch_refresh_tkn(): PASSED\n";
	}
}

void run(Platform overwrite_platform) {
	test_save_and_fetch_access_tkn(overwrite_platform);
	test_save_and_fetch_refresh_tkn(overwrite_platform);
}

}
#endif
