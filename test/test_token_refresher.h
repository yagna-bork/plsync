#ifndef GUARD_TEST_TOKEN_REFRESHER_H
#define GUARD_TEST_TOKEN_REFRESHER_H
#include "../include/token_store.h"
#include "../include/token_refresher.h"
#include "../include/platform.h"
#include <ctime>
#include <iostream>
#include <libcred.hpp>

namespace TestTokenRefresher {

const std::string service = "plsync-token-service";
const Platform platform = Platform::YOUTUBE;
const std::string pwd = "test-token";

void test_access_tkn_valid() {
	save_access_tkn(platform, pwd, 1000);
	YoutubeTokenRefresher ytref;
	if (ytref.access_tkn_valid()) {
		std::cout << "test_access_tkn_valid(): PASSED\n";
	} else {
		std::cout << "test_access_tkn_valid(): FAILED\n";
	}
	std::string err;
	libcred::delete_password(service, "youtube-access-token", &err);
}

void test_access_tkn_expired() {
	save_access_tkn(platform, pwd, -1000);
	YoutubeTokenRefresher ytref;
	if (!ytref.access_tkn_valid()) {
		std::cout << "test_access_tkn_expired(): PASSED\n";
	} else {
		std::cout << "test_access_tkn_expired(): FAILED\n";
	}
	std::string err;
	libcred::delete_password(service, "youtube-access-token", &err);
}

void test_access_tkn_missing() {
	YoutubeTokenRefresher ytref;
	if (!ytref.access_tkn_valid()) {
		std::cout << "test_access_tkn_missing(): PASSED\n";
	} else {
		std::cout << "test_access_tkn_missing(): FAILED\n";
	}
	std::string err;
}

void test_refresh_tkn_valid() {
	save_refresh_tkn(platform, pwd, -1);
	YoutubeTokenRefresher ytref;
	if (ytref.refresh_tkn_valid()) {
		std::cout << "test_refresh_tkn_valid(): PASSED\n";
	} else {
		std::cout << "test_refresh_tkn_valid(): FAILED\n";
	}
	std::string err;
	libcred::delete_password(service, "youtube-refresh-token", &err);
}

void run() {
	test_access_tkn_valid();
	test_access_tkn_expired();
	test_access_tkn_missing();
	test_refresh_tkn_valid();
}

}
#endif
