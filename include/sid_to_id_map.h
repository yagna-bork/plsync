#ifndef GUARD_SHORT_ID_TO_ID_MAP_H
#define GUARD_SHORT_ID_TO_ID_MAP_H
#include "playlist_cache.h"
#include <unordered_map>
#include <string>

using SidToIdMap = std::unordered_map<std::string, std::string>;

SidToIdMap load_sid_to_id_map();
SidToIdMap update_sid_to_id_map(PlaylistCache::Head* head);
void save_sid_to_id_map(const SidToIdMap& map);
#endif
