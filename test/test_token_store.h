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


namespace TestTokenStorage {

const std::string pwd = "test-token";
const std::string TOKEN = "test-token";

void test_save_and_get_access_tkn() {
	std::time_t now = std::time(nullptr);
	if (!save_access_tkn(Platform::TEST, TOKEN, 1000)) {
		std::cout << "test_save_and_get_access_tkn(): FAILED\n";
		return;
	}
	std::string tkn;
	std::shared_ptr<CURL> curl(curl_easy_init(), curl_easy_cleanup);
	if (!get_or_fetch_access_tkn(Platform::TEST, curl, tkn)) {
		std::cout << "test_save_and_get_access_tkn(): FAILED\n";
		return;
	}
	if (tkn != TOKEN) {
		std::cout << "test_save_and_get_access_tkn(): FAILED\n";
	} else {
		std::cout << "test_save_and_get_access_tkn(): PASSED\n";
	}
}

void test_save_and_get_refresh_tkn() {
	if (!save_refresh_tkn(Platform::TEST, TOKEN)) {
		std::cout << "test_save_and_get_refresh_tkn(): FAILED\n";
		return;
	}
	std::string tkn;
	if (!get_refresh_tkn(Platform::TEST, tkn)) {
		std::cout << "test_save_and_get_refresh_tkn(): FAILED\n";
		return;
	}
	if (tkn != TOKEN) {
		std::cout << "test_save_and_get_refresh_tkn(): FAILED\n";
	} else {
		std::cout << "test_save_and_get_refresh_tkn(): PASSED\n";
	}
}

void run() {
	test_save_and_get_access_tkn();
	test_save_and_get_refresh_tkn();
}

}
#endif
