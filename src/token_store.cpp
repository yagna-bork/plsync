#include "../include/token_store.h"
#include <ctime>
#include <string>
#include <algorithm>
#include <libcred.hpp>
#include <stdexcept>

bool save_access_tkn(Platform platform, const std::string &tkn, std::time_t duration) {
	std::string acc = title_lower(platform) + "-access-token";
	std::string expiry = std::to_string(time(nullptr) + duration);
	std::string pwd = tkn + ":" + expiry;
	std::string err;
	return libcred::set_password(KEYCHAIN_SERVICE, acc, pwd, &err) == libcred::SUCCESS;
}

bool save_refresh_tkn(Platform platform, const std::string &tkn) {
	std::string acc = title_lower(platform) + "-refresh-token";
	std::string err;
	return libcred::set_password(KEYCHAIN_SERVICE, acc, tkn, &err) == libcred::SUCCESS;
}

template <class T>
T stot(const std::string &s) {
	const char *type = typeid(T).name();
	if (strcmp(type, "i") == 0) {
		return std::stoi(s);
	} else if (strcmp(type, "l") == 0) {
		return std::stol(s);
	} else if (strcmp(type, "x") == 0) {
		return std::stoll(s);
	} else {
		throw std::domain_error("type must be int, long or long long");
	}
}

bool fetch_access_tkn(Platform platform, std::string &tkn, std::time_t &expiry) {
	std::string acc = title_lower(platform) + "-access-token";
	std::string pass, err;
	if (libcred::get_password(KEYCHAIN_SERVICE, acc, &pass, &err) != libcred::SUCCESS) {
		return false;
	}
	std::string::iterator sep = std::find(pass.begin(), pass.end(), ':');
	tkn = std::string(pass.begin(), sep);
	std::string expiry_str(sep+1, pass.end());
	expiry = stot<std::time_t>(expiry_str);
	return true;
}

bool fetch_refresh_tkn(Platform platform, std::string &tkn) {
	std::string acc = title_lower(platform) + "-refresh-token";
	std::string err;
	return libcred::get_password(KEYCHAIN_SERVICE, acc, &tkn, &err) == libcred::SUCCESS;
}
