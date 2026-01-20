#include "../include/config.h"
#include <unordered_map>
#include <fstream>
#include <string>
#include <algorithm>

static std::unordered_map<std::string, std::string> CONFIG;
// TODO change for prod
static std::string CONFIG_PATH = "plsync.cfg";

void load_config() {
	// TODO raise exception if something anything wrong
	std::ifstream config(CONFIG_PATH);
	std::string line, name, val;
	std::string::iterator sep;
	while(std::getline(config, line)) {
		sep = std::find(line.begin(), line.end(), '=');
		name = std::string(line.begin(), sep);
		val = std::string(sep+1, line.end());
		CONFIG[name] = val;
	}
}

std::string get_setting(const std::string &name) {
	if (CONFIG.empty()) {
		load_config();
	}
	if (!CONFIG.count(name)) {
		return "";
	}
	return CONFIG[name];
}
