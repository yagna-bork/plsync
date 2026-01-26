#ifndef GUARD_TEST_TOKEN_STORE_H
#define GUARD_TEST_TOKEN_STORE_H
#include "../include/token_store.h"
#include <string>
#include <iostream>
#include <libcred.hpp>

// types not included in token_store.h
bool save_tkn(const std::string&, const std::string&, std::time_t);
bool fetch_tkn(const std::string&, std::string&, std::time_t&);

namespace TestTokenStorage {

const std::string service = "plsync-token-service";
const std::string acc = "test-tkn";
const std::string pwd = "test-password";

void test_save_tkn() {
	bool success = save_tkn(acc, pwd, -1);
	std::string saved, err, expected = pwd + ":-1";
	libcred::get_password(service, acc, &saved, &err);

	if (!success) {
		std::cout << "test_save_token(): FAILED" << '\n';
	} else if (saved == expected) {
		std::cout << "test_save_token(): PASSED" << '\n';
	} else {
		std::cout << "test_save_token(): FAILED" << '\n';
		std::cout << "Expected: " << expected << '\n';
		std::cout << "Actual: " << saved << '\n';
	}
	libcred::delete_password(service, acc, &err);
}

void test_fetch_tkn() {
	std::string tkn, err;
	std::time_t expiry, expected_expiry = 1000;
	libcred::set_password(service, acc, pwd+":"+std::to_string(expected_expiry), &err);

	if (!fetch_tkn(acc, tkn, expiry)) {
		std::cout << "test_fetch_tkn(): FAILED\n";
	} else if (tkn != pwd) {
		std::cout << "test_fetch_tkn(): FAILED\n" << "Expected token: " 
				  << pwd << "\nActual: " << tkn << '\n';
	} else if (expiry != expected_expiry) {
		std::cout << "test_fetch_tkn(): FAILED\n" << "Expected expiry: " << expected_expiry 
				  << "\nActual: " << expiry << '\n';
	} else {
		std::cout << "test_fetch_tkn(): PASSED\n";
	}
	libcred::delete_password(service, acc, &err);
}

void test_tokens_exist() {
	tokens_exist();
}

void run() {
	test_save_tkn();
	test_fetch_tkn();
	test_tokens_exist();
}

}
#endif
