#include "test_token_store.h"
#include "test_token_refresher.h"
#include "test_api.h"
#include "../include/platform.h"
#include <iostream>
#include <string>

typedef std::vector<libcred::Credentials> Credentials;

Credentials copy_existing_credentials(Platform platform) {
	Credentials creds;
	std::string err;
	if (libcred::find_credentials(KEYCHAIN_SERVICE, &creds, &err) != libcred::SUCCESS) {
		throw std::runtime_error("failed to copy existing tokens");
	}
	Credentials platform_creds;
	std::copy_if(creds.begin(), creds.end(), std::back_inserter(platform_creds), 
		[&platform](const libcred::Credentials &c) { 
			return contains(c.first, title_lower(platform)); 
		}
	);
	return platform_creds;
}

void paste_existing_credentials(const Credentials &creds) {
	std::string err;
	for (const auto &cred: creds) {
		if (libcred::set_password(KEYCHAIN_SERVICE, cred.first, cred.second, &err) != libcred::SUCCESS) {
			throw std::runtime_error("couldn't paste existing token");
		}
	}
}

int main() {
	// copy any user generated youtube token
	// and paste them back in later so tests
	// can overwrite them without consequence
	Platform overwrite_platform = Platform::YOUTUBE;
	Credentials creds = copy_existing_credentials(overwrite_platform);
	
	std::cout << "Token storage tests\n";
	std::cout << "-------------------\n";
	TestTokenStorage::run(overwrite_platform);

	std::cout << "\n\nToken refresher tests\n";
	std::cout << "---------------------\n";
	TestTokenRefresher::run(overwrite_platform);

	std::cout << "\n\nYoutube API tests\n";
	std::cout << "-----------------\n";
	TestAPI::run();

	paste_existing_credentials(creds);
	return 0;
}
