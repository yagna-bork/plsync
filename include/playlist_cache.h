#ifndef GUARD_PLAYLIST_CACHE_H 
#define GUARD_PLAYLIST_CACHE_H 
#include "models.h"
#include "platform.h"
#include <vector>

struct CacheNode {
	Playlist playlist;
	bool is_tracked;
	CacheNode* next;
	bool was_changed;
};

struct CacheHead {
	CacheNode* next;
	bool was_changed;
};

CacheHead* load_cache(Platform plat);
void update_cache(CacheHead* head, std::vector<Playlist>& playlists);
void free_cache(CacheHead* head, Platform plat);
#endif
