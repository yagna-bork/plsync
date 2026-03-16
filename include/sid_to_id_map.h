#ifndef GUARD_SHORT_ID_TO_ID_MAP_H
#define GUARD_SHORT_ID_TO_ID_MAP_H
#include "playlist_cache.h"
#include "platform.h"
#include <unordered_map>
#include <string>
#include <stdexcept>

using SidToIdMap = std::unordered_map<std::string, std::string>;

class SidToIdMapUninitialisedError : public std::domain_error {
public:
	SidToIdMapUninitialisedError(): std::domain_error("lookup attempted before map was initialised") {}
};

SidToIdMap load_sid_to_id_map(Platform plat);
std::string sid_to_id_lookup(const std::string& sid, Platform plat);
SidToIdMap update_sid_to_id_map(PlaylistCache::Head* head, Platform plat);
void save_sid_to_id_map(const SidToIdMap& map, Platform plat);
#endif
