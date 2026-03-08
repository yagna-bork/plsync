#include "../include/sid_to_id_map.h"
#include "../include/sid_to_id_map.pb.h"
#include "../include/playlist_cache.h"
#include "../include/config.h"
#include "../include/platform.h"
#include "../include/util.h"
#include <cstddef>
#include <fstream>
#include <ios>
#include <filesystem>
#include <string>

std::size_t NUM_BUCKETS = 500;

using Map = SidToIdMap;
using ProtoMap = proto::SidToIdMap;
namespace fs = std::filesystem;
namespace Cache = PlaylistCache;

fs::path file_path(Platform plat) {
	return fs::path(get_setting("cache_dir")) / "sid_to_id_map" / platform_title_lower(plat);
}

Map load_sid_to_id_map(Platform plat) {
	ProtoMap proto_map;
	{
		auto file = ensure_file<std::ifstream>(file_path(plat), std::ios::binary);
		proto_map.ParseFromIstream(&file);
	}

	Map map;
	for (const auto& bucket: proto_map.buckets()) {
		for (const auto& pair: bucket.pairs()) {
			std::string short_id(pair.short_id());
			std::string id(pair.id());
			map[short_id] = id;
		}
	}
	return map;
}

std::string sid_to_id_lookup(const std::string& sid, Platform plat) {
	ProtoMap proto_map;
	{
		auto file = ensure_file<std::ifstream>(file_path(plat), std::ios::binary);
		proto_map.ParseFromIstream(&file);
	}

	auto sid_hash = std::hash<std::string>{}(sid);
	auto bucket = sid_hash % NUM_BUCKETS;
	for (const auto& pair: proto_map.buckets(bucket).pairs()) {
		if (pair.short_id() != sid) continue;
		return std::string(pair.id());
	}
	return "";
}

Map update_sid_to_id_map(Cache::Head* head, Platform plat) {
	Map map;
	for (auto it = Cache::cbegin(head); it != Cache::cend(); ++it) {
		map[it->short_id] = it->id;
	}
	save_sid_to_id_map(map, plat);
	return map;
}

void save_sid_to_id_map(const Map& map, Platform plat) {
	ProtoMap proto_map;
	for (std::size_t i = 0; i != NUM_BUCKETS; i++) {
		proto_map.add_buckets();
	}

	for (const auto& pair: map) {
		auto short_id_hash = std::hash<std::string>{}(pair.first);
		std::size_t bucket = short_id_hash % NUM_BUCKETS; 
		auto* proto_pair = proto_map.mutable_buckets(bucket)->add_pairs();
		proto_pair->set_short_id(pair.first);
		proto_pair->set_id(pair.second);
	}
	
	auto file = ensure_file<std::ofstream>(file_path(plat), std::ios::binary);
	proto_map.SerializeToOstream(&file);
}
