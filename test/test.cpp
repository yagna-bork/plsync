#include "test_token_store.h"
#include "test_token_refresher.h"
#include "test_api.h"
#include <iostream>

int main() {
	std::cout << "Token storage tests\n";
	std::cout << "-------------------\n";
	TestTokenStorage::run();

	std::cout << "\n\nToken refresher tests\n";
	std::cout << "---------------------\n";
	TestTokenRefresher::run();

	std::cout << "\n\nYoutube API tests\n";
	std::cout << "-----------------\n";
	TestAPI::run();
	return 0;
}
