#include "../include/token_store.h"
#include "../include/api.h"
#include <ctime>
#include <cassert>
#include <string>
#include <iostream>
#include <algorithm>
#include <libcred.hpp>
#include <stdexcept>
#include <memory>

bool save_access_tkn(Platform platform, const std::string &tkn, std::time_t duration) {
	std::string acc = platform_title_lower(platform) + "-access-token";
	std::string expiry = std::to_string(time(nullptr) + duration);
	std::string pwd = tkn + ":" + expiry;
	std::string err;
	return libcred::set_password(KEYCHAIN_SERVICE, acc, pwd, &err) == libcred::SUCCESS;
}

bool save_refresh_tkn(Platform platform, const std::string &tkn) {
	std::string acc = platform_title_lower(platform) + "-refresh-token";
	std::string err;
	return libcred::set_password(KEYCHAIN_SERVICE, acc, tkn, &err) == libcred::SUCCESS;
}

template <class T>
static T stot(const std::string &s) {
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

static bool get_access_tkn(Platform platform, std::string &tkn, std::time_t &expiry) {
	std::string acc = platform_title_lower(platform) + "-access-token";
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

static bool get_access_tkn(Platform platform, std::string &tkn) {
	std::time_t _;
	return get_access_tkn(platform, tkn, _);
}

static bool get_access_tkn(Platform platform, std::time_t &expiry) {
	std::string  _;
	return get_access_tkn(platform, _, expiry);
}

bool get_refresh_tkn(Platform platform, std::string &tkn) {
	std::string acc = platform_title_lower(platform) + "-refresh-token";
	std::string err;
	return libcred::get_password(KEYCHAIN_SERVICE, acc, &tkn, &err) == libcred::SUCCESS;
}

static bool is_access_tkn_valid(Platform platform) {
	std::time_t expiry;
	if(!get_access_tkn(platform, expiry)) {
		// not in keychain
		return false;
	}
	return expiry > time(nullptr);
}

bool is_refresh_tkn_valid(Platform platform) {
	std::string _;
	return get_refresh_tkn(platform, _);
}

bool get_or_fetch_access_tkn(Platform platform, std::shared_ptr<CURL> curl, std::string &tkn) {
	if (is_access_tkn_valid(platform)) {
		return get_access_tkn(platform, tkn);
	}

	std::string refresh_tkn;
	if (!get_refresh_tkn(platform, refresh_tkn)) {
		return false;
	}

	std::unique_ptr<BaseAuthAPI> api = BaseAuthAPI::get_api(platform, curl);
	BaseAuthAPI::AccessTokenResponse resp;
	try {
		resp = api->refresh_access_tkn(refresh_tkn);
	} catch (const BaseAuthAPI::RequestError &e) { 
		return false;
	}

	save_access_tkn(platform, resp.access_tkn, resp.access_duration);
	tkn = std::move(resp.access_tkn);
	if (platform == Platform::SPOTIFY && !save_refresh_tkn(platform, resp.refresh_tkn)) {
		return false;
	}
	return true;
}

std::string get_or_refresh_access_tkn(Platform platform, std::shared_ptr<CURL> curl) {
	std::string tkn;
	if (is_access_tkn_valid(platform)) {
		if (!get_access_tkn(platform, tkn)) {
			throw TokenStorageAccessError();
		}
		return tkn;
	}

	std::string refresh_tkn;
	if (!get_refresh_tkn(platform, refresh_tkn)) {
		throw TokenStorageAccessError();
	}
	std::unique_ptr<BaseAuthAPI> api = BaseAuthAPI::get_api(platform, curl);
	BaseAuthAPI::AccessTokenResponse resp = api->refresh_access_tkn(refresh_tkn);
	save_access_tkn(platform, resp.access_tkn, resp.access_duration);
	if (platform == Platform::SPOTIFY && !save_refresh_tkn(platform, resp.refresh_tkn)) {
		throw TokenStorageAccessError();
	}
	return std::move(resp.access_tkn);
}
