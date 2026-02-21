#include "test_token_store.h"
#include "test_api.h"
#include "test_meta_cache.h"
#include "../include/platform.h"
#include <iostream>
#include <string>

int main() {
	std::cout << "Token storage tests\n";
	std::cout << "-------------------\n";
	TestTokenStorage::run();

	std::cout << "\n\nYoutube API tests\n";
	std::cout << "-----------------\n";
	TestAPI::run();

	std::cout << "\n\nMeta cache tests\n";
	std::cout << "-----------------\n";
	TestMetaCache::run();
	return 0;
}
