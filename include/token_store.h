#ifndef GUARD_TOKEN_STORE_H
#define GUARD_TOKEN_STORE_H
#include <ctime>
#include <string>

bool save_access_tkn(const std::string &platform, const std::string &tkn, std::time_t duration);

bool save_refresh_tkn(const std::string &platform, const std::string &tkn, std::time_t duration);

bool fetch_access_tkn(const std::string &platform, std::string &tkn, std::time_t &expiry);

bool fetch_refresh_tkn(const std::string &platform, std::string &tkn, std::time_t &expiry);
#endif
