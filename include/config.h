#ifndef GUARD_CONFIG_H
#define GUARD_CONFIG_H
#include "platform.h"
#include <string>

std::string get_setting(std::string name);

std::string get_setting(std::string name, Platform platform);
#endif
