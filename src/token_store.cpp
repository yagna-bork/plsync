#include "../include/token_store.h"
#include <cassert>
#include <ctime>
#include <string>
#include <iostream>
#include <algorithm>
#include <libcred.hpp>
#include <stdexcept>

const std::string SERVICE = "plsync-token-service";

bool save_tkn(const std::string &acc, const std::string &tkn, std::time_t duration) {
	std::string expiry = "-1";
	if (duration != -1) {
		expiry = std::to_string(time(nullptr) + duration);
	}
	std::string pass = tkn + ":" + expiry, err;
	return libcred::set_password(SERVICE, acc, pass, &err) == libcred::SUCCESS;
}

bool save_access_tkn(const std::string &platform, const std::string &tkn, std::time_t duration) {
	std::string acc = std::string(platform=="yt" ? "youtube" : "spotify") + "-access-token";
	return save_tkn(acc, tkn, duration);
}

bool save_refresh_tkn(const std::string &platform, const std::string &tkn, std::time_t duration) {
	std::string acc = std::string(platform=="yt" ? "youtube" : "spotify") + "-refresh-token";
	return save_tkn(acc, tkn, duration);
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

bool fetch_tkn(const std::string &acc, std::string &tkn, std::time_t &expiry) {
	std::string pass, err;
	if (libcred::get_password(SERVICE, acc, &pass, &err) != libcred::SUCCESS) {
		return false;
	}
	std::string::iterator sep = std::find(pass.begin(), pass.end(), ':');
	tkn = std::string(pass.begin(), sep);
	expiry = stot<std::time_t>(std::string(sep+1, pass.end()));
	return true;
}

bool fetch_access_tkn(const std::string &platform, std::string &tkn, std::time_t &expiry) {
	std::string acc = std::string(platform == "yt" ? "youtube" : "spotify") + "-access-token";
	return fetch_tkn(acc, tkn, expiry);
}

bool fetch_refresh_tkn(const std::string &platform, std::string &tkn, std::time_t &expiry) {
	std::string acc = std::string(platform == "yt" ? "youtube" : "spotify") + "-refresh-token";
	return fetch_tkn(acc, tkn, expiry);
}
