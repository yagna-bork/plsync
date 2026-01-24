#include "../include/token_store.h"
#include <ctime>
#include <string>
#include <libcred.hpp>
#include <iostream>

bool save_tkn(const std::string &acc, const std::string &tkn, std::time_t duration) {
	std::string expiry = "-1";
	if (duration != -1) {
		expiry = std::to_string(time(nullptr) + duration);
	}
	std::string pass = tkn + ":" + expiry, err;
	return libcred::set_password("plsync-token-service", acc, pass, &err) == libcred::SUCCESS;
}

bool save_access_tkn(const std::string &platform, const std::string &tkn, std::time_t duration) {
	std::string acc = std::string(platform=="yt" ? "youtube" : "spotify") + "-access-token";
	return save_tkn(acc, tkn, duration);
}

bool save_refresh_tkn(const std::string &platform, const std::string &tkn, std::time_t duration) {
	std::string acc = std::string(platform=="yt" ? "youtube" : "spotify") + "-refresh-token";
	return save_tkn(acc, tkn, duration);
}

bool fetch_access_tkn(const std::string &platform, std::string &tkn, std::time_t &expiry);

bool fetch_refresh_tkn(const std::string &platform, std::string &tkn, std::time_t &expiry);
