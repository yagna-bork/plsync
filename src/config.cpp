#include "../include/config.h"
#include "../include/client_secret.h"
#include "../include/platform.h"
#include <cassert>
#include <unordered_map>
#include <fstream>
#include <string>
#include <algorithm>
#include <stdexcept>

static std::unordered_map<std::string, std::string> CONFIG;
// TODO change for prod
static std::string CONFIG_PATH = "plsync.cfg";

void load_config() {
	// TODO raise exception if something anything wrong
	std::ifstream config(CONFIG_PATH);
	if (!config) {
		throw std::runtime_error("Config file not found. Reinstall plsync");
	}
	std::string line, name, val;
	std::string::iterator sep;
	while(std::getline(config, line)) {
		sep = std::find(line.begin(), line.end(), '=');
		name = std::string(line.begin(), sep);
		val = std::string(sep+1, line.end());
		CONFIG[name] = val;
	}
	// fucking google...
	CONFIG["yt_client_secret"] = CLIENT_NOT_SO_SECRET;
}

std::string get_setting(std::string name, const std::string prefix) {
	if (CONFIG.empty()) {
		load_config();
	}
	name = prefix + name;
	assert(CONFIG.count(name) != 0);
	return CONFIG[name];
}

std::string get_setting(std::string name) {
	return get_setting(name, /*prefix=*/"");
}

std::string get_setting(std::string name, Platform platform) {
	return get_setting(name, abbrev(platform) + "_");
}
