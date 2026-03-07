#include "../include/sid_to_id_map.h"
#include "../include/sid_to_id_map.pb.h"
#include "../include/playlist_cache.h"
#include "../include/config.h"
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

fs::path file_path() {
	return fs::path(get_setting("cache_dir")) / "short_id_to_id_map";
}

Map load_sid_to_id_map() {
	std::ifstream file(file_path(), std::ios::binary);
	ProtoMap proto_map;
	proto_map.ParseFromIstream(&file);

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

Map update_sid_to_id_map(Cache::Head* head) {
	Map map;
	for (auto it = Cache::cbegin(head); it != Cache::cend(); ++it) {
		map[it->short_id] = it->id;
	}
	save_sid_to_id_map(map);
	return map;
}

void save_sid_to_id_map(const Map& map) {
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
	
	std::ofstream file(file_path(), std::ios::binary);
	proto_map.SerializeToOstream(&file);
}
