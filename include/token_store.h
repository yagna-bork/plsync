#ifndef GUARD_TOKEN_STORE_H
#define GUARD_TOKEN_STORE_H
#include "platform.h"
#include <ctime>
#include <string>

bool save_access_tkn(Platform platform, const std::string &tkn, std::time_t duration);

bool save_refresh_tkn(Platform platform, const std::string &tkn, std::time_t duration);

bool fetch_access_tkn(Platform platform, std::string &tkn, std::time_t &expiry);

bool fetch_refresh_tkn(Platform platform, std::string &tkn, std::time_t &expiry);
#endif
