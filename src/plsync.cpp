#include "../include/api.h"
#include "../include/platform.h"
#include "../include/util.h"
#include "../include/new_api.h"
#include "../include/cache.pb.h"
#include "../include/cache.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <string>
#include <vector>

namespace fs = std::filesystem;

const std::vector<const char *> COMMANDS = {"init", "untracked", "track", "tracked", "untrack", "sync"};

/* init-start */
const std::string init_description = "Allow OAuth permissions for Youtube and Spotify. Initially the only valid command.";

bool get_user_permissions(Platform platform, std::shared_ptr<CURL> curl) {
	auto api = BaseAuthAPI::get_api(platform, curl);

	// direct user to permission screen
	std::ostringstream cmd;
	cmd << "open '" << api->get_auth_url() << "'";
	system(cmd.str().c_str());

	// listen at redirect url for auth_code
	std::string auth_code;
	std::cout << "Waiting for " << platform_title(platform) << " authentication code... " << std::flush;
	if (!api->collect_auth_code()) {
		std::cout << "Unable to complete " << platform_title(platform) << " authentication. Please try again" << '\n';
		return false;
	}
	std::cout << "Got it!\n";

	// exchange auth_code for access & refresh tokens
	BaseAuthAPI::TokenResponse tkn_resp;
	try {
		tkn_resp = api->exchange_auth_code();
	} catch (const BaseAuthAPI::RequestError &e) {
		std::cerr << "Something went wrong. Please try again\n";
		return false;
	}

	// finally store the tokens
	if (!save_access_tkn(platform, tkn_resp.access_tkn, tkn_resp.access_duration)) {
		std::cerr << "Couldn't store tokens in keychain. Please try again" << '\n';
		return false;
	}
	if (!save_refresh_tkn(platform, tkn_resp.refresh_tkn)) {
		std::cerr << "Couldn't store tokens in keychain. Please try again" << '\n';
		return false;
	}
	std::cout << "Success! " << platform_title(platform) << " authentication completed" << '\n';
	return true;
}

int run_init(bool init_youtube, bool init_spotify) {
	auto curl = get_curl();
	if (init_youtube && !get_user_permissions(Platform::YOUTUBE, curl)) {
		return 1;
	}
	if (init_spotify && !get_user_permissions(Platform::SPOTIFY, curl)) {
		return 1;
	}
	return 0;
}





/* untracked-start */
const std::string untracked_description = "Display information about playlists which are not being tracked";

static void print_untracked_usage() {
	std::cout << "usage: plsync untracked <platform>\n\n"
			  << untracked_description << "\n\n"
			  << "Options:\n"
			  << "  platform  Name of platform to view playlists from. Can either be 'yt' or a prefix of 'youtube' and 'spotify'\n";
}

static Platform parse_untracked_args(int argc, char *argv[]) {
	if (argc != 1 || strcmp(argv[0], "-h")==0 || strcmp(argv[0], "--help")==0) {
		print_untracked_usage();
		exit(1);
	}
	auto platform = parse_platform(argv[0]);
	if (platform == Platform::INVALID) {
		print_untracked_usage();
		exit(1);
	}
	return platform;
}

int run_untracked(int argc, char *argv[]) {
	Platform platform = parse_untracked_args(argc, argv);
	std::string tkn;
	std::shared_ptr<CURL> curl = get_curl();
	if (!get_or_fetch_access_tkn(platform, curl, tkn)) {
		std::cerr << "Couldn't get access token. Please try again\n";
		return 1;
	}
	std::unique_ptr<BaseDataAPI> api = BaseDataAPI::get_api(platform, curl, tkn);

	PlaylistCache::Handle cache(platform);
	bool modified;
	std::vector<Playlist> modified_playlists;
	std::string modified_etag = cache.head->etag;
	try {
		modified = api->get_playlists(modified_playlists, modified_etag);
	} catch (const BaseAPI::RequestError &e) {
		std::cerr << "Something went wrong. Try again.\n";
		return 1;
	}
	if (modified) {
		PlaylistCache::update(cache.head, cache.plat, modified_playlists, modified_etag);
	}

	bool were_sids_modified = modified;
	int new_sid_len = PlaylistCache::calculate_short_id_len(cache.head);
	if (new_sid_len != cache.head->sid_len) {
		PlaylistCache::update_short_ids(cache.head, new_sid_len);
		were_sids_modified = true;
	}
	if (were_sids_modified) {
		update_sid_to_id_map(cache.head, cache.plat);
	}

	size_t longest_title = 0;
	for (const Playlist& pl: cache) {
		longest_title = std::max(longest_title, utf8_len(pl.title));
	}

	int id_pad = std::max(1, static_cast<int>(cache.head->sid_len) * 2 - 1);
	int title_pad = std::max(size_t(1), longest_title - 4);
	std::stringstream heading_ss;
	heading_ss << "Id" << std::string(id_pad, ' ') 
		       << "Title" << std::string(title_pad, ' ') 
		       << "Privacy " << "Items";
	std::string heading = heading_ss.str();
	std::cout << heading << '\n';
	std::cout << std::string(heading.size(), '-') << '\n';

	// TODO sort
	// TODO use setw but also use manual padding with title
	for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
		if (!it.ptr.node->items_id.empty()) {
			continue;
		}
		id_pad = std::max(1, 3 - static_cast<int>(cache.head->sid_len) * 2); 
		int title_pad = std::max(longest_title, size_t(5)) + 1 - utf8_len(it->title);
		std::string privacy_type = it->is_private ? "private" : "public";
		int privacy_pad = it->is_private ? 1 : 2;
		std::cout << bin_to_hex(it->short_id) << std::string(id_pad, ' ')
				  << it->title << std::string(title_pad, ' ')
				  << privacy_type << std::string(privacy_pad, ' ')
				  << it->items << '\n';
	}
	return 0;
}





/* track-start */
const std::string track_description = "Start tracking an untracked playlist";

static void print_track_usage() {
	std::cout << "usage: plsync track <platform> <playlist-id>\n\n"
			  << track_description << "\n\n"
			  << "Options:\n"
			  << "  platform     Name of platform to view playlists from. Can either be 'yt' or a prefix of 'youtube' and 'spotify'\n"
			  << "  playlist-id  Value of the id field shown in the output of <untracked> for the playlist to track\n";
}

static std::unordered_map<Platform, std::string> parse_track_args(int argc, char *argv[]) {
	if (argc < 3) {
		throw std::invalid_argument("");
	}

	// TODO change input format to unambiguous plat,[sid]
	// because an sid could look like a platform e.g. yt
	// and confuse the parsing algo
	std::unordered_map<Platform, std::string> plat_to_sid;
	Platform prev_plat = Platform::INVALID;
	for (int i = 0; i != argc; i++) {
		auto plat = parse_platform(argv[i]);
		if (plat != Platform::INVALID) {
			if (plat_to_sid.count(plat)) {
				throw std::invalid_argument("");
			}
			plat_to_sid[plat] = "";
			prev_plat = plat;
			continue;
		}
		if (prev_plat == Platform::INVALID || !plat_to_sid[prev_plat].empty() || std::strlen(argv[i]) % 2 != 0) {
			throw std::invalid_argument("");
		}
		plat_to_sid[prev_plat] = argv[i];
	}

	auto sid_empty = [](const std::pair<Platform, std::string>& pair) { return pair.second.empty(); };
	if (plat_to_sid.size() < 2 || std::all_of(plat_to_sid.begin(), plat_to_sid.end(), sid_empty)) {
		throw std::invalid_argument("");
	}
	return plat_to_sid;
}

static void track(int argc, char *argv[]) {
	std::unordered_map<Platform, std::string> plat_to_sid = parse_track_args(argc, argv);
	std::shared_ptr<CURL> curl = get_curl();
	std::unordered_map<Platform, PlaylistCache::Node> plat_to_node;
	std::string items_id;
	std::string playlist_title;
	for (const auto& pair: plat_to_sid) {
		auto plat = pair.first;
		const auto& sid = pair.second;
		if (sid.empty()) {
			continue;
		}

		// check sid is valid
		std::string id = sid_to_id_lookup(hex_to_bin(sid), plat);
		PlaylistCache::Node node = PlaylistCache::load_node(id, plat);
	
		// check playlist wasn't deleted
		std::string access_tkn = get_or_refresh_access_tkn(plat, curl);
		Playlist modified_playlist;
		bool modified = API::get_playlist(plat, curl.get(), access_tkn, node.playlist.id, node.playlist.etag, modified_playlist);
		if (modified) {
			if (modified_playlist.id.empty()) {
				// playlist was deleted
				remove_node(node, plat);
				throw std::invalid_argument("");
			} else {
				node.playlist = std::move(modified_playlist);
				save_node(node, plat);
			}
		}
		
		// check at most one playlist is already tracked
		if (!node.items_id.empty()) {
			if (!items_id.empty()) {
				throw std::invalid_argument("");
			} else {
				items_id = node.items_id;
			}
		}
		if (playlist_title.empty()) {
			playlist_title = node.playlist.title;
		}
		plat_to_node[plat] = std::move(node);
	}
	
	// TODO check no duplication of platforms
	PlaylistItems pl_items;
	if (items_id.empty()) {
		pl_items.id = bin_to_hex(rndstr(16));
	} else {
		pl_items = load_playlist_items(items_id);
	}

	// create playlists for platforms where it wasn't provided
	for (const auto& pair: plat_to_sid) {
		if (!pair.second.empty()) {
			continue;
		}
		Platform plat = pair.first;
		std::string access_tkn = get_or_refresh_access_tkn(plat, curl);
		Playlist playlist = API::create_playlist(plat, curl.get(), access_tkn, playlist_title);
		PlaylistCache::Node node(playlist);
		create_node(node, plat);
		plat_to_node[plat] = std::move(node);
	}
	
	// and finally track the provided playlists
	for (auto& pair: plat_to_node) {
		auto& plat = pair.first;
		auto& node = pair.second;
		if (!node.items_id.empty()) {
			continue;
		}
		pl_items.tracked.emplace_back(plat, Playlist(node.playlist.id, /*items_etag=*/""));
		node.items_id = pl_items.id;
		save_node(node, plat);
	}
	save_playlist_items(pl_items);
}

int run_track(int argc, char *argv[]) {
	try {
		track(argc, argv);
		return 0;
	} catch (const std::invalid_argument& e) {
		print_track_usage();
	} catch (const SidOutOfRangeError& e) {
		print_track_usage();
	} catch (const std::exception& e) {
		std::cerr << "Something went wrong please try again\n";
	}
	return 1;
}





/* tracked-start */
const std::string tracked_description = "Display information about which playlists are being tracked";

static void print_tracked_usage() {
	std::cout << "usage: plsync tracked\n\n" << tracked_description << '\n';
}

static bool parse_tracked_args(int argc, char* argv[]) {
	if (argc > 0) {
		print_tracked_usage();
		return false;
	}
	return true;
}

int save_cache_quit(PlaylistItemsCache& cache) {
	save_playlist_items_cache(cache);
	std::cerr << "Something went wrong. Please try again.\n";
	return 1;
}

int run_tracked(int argc, char* argv[]) {
	if (!parse_tracked_args(argc, argv)) {
		return 0;
	}
	std::vector<std::size_t> plat_to_sid_len(Platform::INVALID, 0);
	size_t longest_sid = 0;
	for (int i = 0; i != Platform::INVALID; i++) {
		Platform plat = static_cast<Platform>(i);
		PlaylistCache::Head head;
		if (PlaylistCache::load_head(plat, head)) {
			plat_to_sid_len[plat] = head.sid_len;
			longest_sid = std::max(longest_sid, head.sid_len);
		}
	}

	auto curl = get_curl();
	PlaylistItemsCache cache = load_playlist_items_cache();
	try {
		update_playlist_items_cache(cache, curl, get_access_tokens(curl));
	} catch (const TokenStorageAccessError& e) {
		return save_cache_quit(cache);
	} catch (const API::RequestError& e) {
		return save_cache_quit(cache);
	}

	size_t id_pad = longest_sid*2 - 1;
	std::ostringstream heading_ss;
	heading_ss << "Platform Id" << std::string(id_pad, ' ') << "Title";
	std::string heading = heading_ss.str();
	std::cout << heading << '\n' << std::string(heading.size(), '-') << '\n';

	bool skip_newline = true;
	for (const auto& pl_items: cache) {
		if (skip_newline) {
			skip_newline = false;
		} else {
			std::cout << '\n';
		}
		for (const auto& [plat, pl]: pl_items.tracked) {
			size_t plat_pad = 9 - platform_title(plat).size();
			id_pad = (longest_sid - pl.short_id.size())*2 + 1;
			std::cout << platform_title(plat) << std::string(plat_pad, ' ') 
					  << bin_to_hex(pl.short_id) << std::string(id_pad, ' ')
					  << pl.title << '\n';
		}
		std::cout << pl_items.song_hashes.size() << " song(s)\n";
	}
	save_playlist_items_cache(cache);
	return 0;
}





/* untrack-start */
const std::string untrack_description = "Stop tracking a tracked playlist";

static void print_untrack_usage() {
	std::cout << "usage: plsync untrack <platform> <playlist-id>\n\n"
			  << untrack_description << "\n\n"
			  << "Options:\n"
			  << "  platform     The platform that the playlist belongs to. Can either be 'yt' or a prefix of 'youtube' and 'spotify'\n"
			  << "  playlist-id  Value of the id field shown in the output of <tracked> for the playlist to untrack\n";
}

static void parse_untrack_args(int argc, char* argv[], Platform& plat, PlaylistCache::Node& node) {
	if (argc != 2) {
		throw std::invalid_argument("");
	}
	if (parse_platform(argv[0]) == Platform::INVALID) {
		throw std::invalid_argument("");
	}
	if (strlen(argv[1]) == 0 || strlen(argv[1]) % 2 != 0) {
		throw std::invalid_argument("");
	}
	plat = parse_platform(argv[0]);
	std::string playlist_id = sid_to_id_lookup(hex_to_bin(argv[1]), plat);
	if (playlist_id.empty()) {
		throw std::invalid_argument("");
	}
	node = PlaylistCache::load_node(playlist_id, plat);
	if (node.items_id.empty()) {
		throw std::invalid_argument("");
	}
}

int untrack(int argc, char* argv[]) {
	Platform plat;
	PlaylistCache::Node node;
	parse_untrack_args(argc, argv, plat, node);
    auto pl_items = load_playlist_items(node.items_id);
	if (pl_items.tracked.size() == 2) {
		remove_playlist_items(pl_items.id);
	} else {
		for (auto it = pl_items.tracked.begin(); it != pl_items.tracked.end(); it++) {
			if (it->second.id == node.playlist.id) {
				pl_items.tracked.erase(it);
				break;
			}
		}
		save_playlist_items(pl_items);
	}
	node.items_id.clear();
	PlaylistCache::save_node(node, plat);
	return 0;
}

int run_untrack(int argc, char* argv[]) {
	try {
		return untrack(argc, argv);
	} catch (const std::invalid_argument& e) {
		print_untrack_usage();
		return 1;
	} catch (const SidOutOfRangeError& e) {
		print_untrack_usage();
		return 1;
	}
}




/* sync-start */
const std::string sync_description = "Sync your tracked playlists";

void print_sync_usage() {
	std::cout << "usage: plsync sync\n\n" << sync_description << '\n';
}

static bool parse_sync_args(int argc, char* argv[]) {
	if (argc > 0) {
		print_sync_usage();
		return false;
	}
	return true;
}

int sync(PlaylistItemsCache& cache) {
	std::cout << "Syncing...";
	std::shared_ptr<CURL> curl = get_curl();
	std::vector<std::string> plat_to_access_tkn = get_access_tokens(curl);
	update_playlist_items_cache(cache, curl, plat_to_access_tkn);

	for (PlaylistItems& pl_items: cache) {
		for (std::pair<Platform, Playlist>& p: pl_items.tracked) {
			std::vector<API::Song> songs;
			bool modified = API::get_playlist_items(
				p.first, curl.get(), plat_to_access_tkn[p.first], p.second.id, songs, p.second.items_etag
			);
		}
	}
	std::cout << " Done!\n";
	return 0;
}

int run_sync(int argc, char* argv[]) {
	if (!parse_sync_args(argc, argv)) {
		return 0;
	}
	PlaylistItemsCache cache = load_playlist_items_cache();
	try {
		return sync(cache);
	} catch (const TokenStorageAccessError& e) {
		return save_cache_quit(cache);
	} catch (const API::RequestError& e) {
		return save_cache_quit(cache);
	}
}





/* plsync-start */
static void print_plsync_init_only_usage() {
	std::cout << "usage: plsync [-h] <command>\n\n" 
			  << "Avaliable commands:\n" 
			  << "  init  Allow OAuth permissions for Youtube and Spotify. Initially the only valid command"
			  << std::endl;
}

static void print_plsync_usage() {
	std::cout << "usage: plsync [-h] <command>\n\n" 
			  << "Avaliable commands:\n" 
			  << "  init       " << init_description << "\n\n"
			  << "  untracked  " << untracked_description << "\n\n"
			  << "  track      " << track_description << "\n\n"
			  << "  tracked    " << tracked_description << "\n\n"
			  << "  untrack    " << untrack_description << "\n\n"
			  << "  sync       " << sync_description << '\n';
}

const char *parse_plsync_args_init_only(int argc, char *argv[]) {
	if (argc < 2 || strcmp(argv[1], "-h")==0 || strcmp(argv[1], "--help")==0) {
		print_plsync_init_only_usage();
		exit(1);
	}
	if (strcmp(argv[1], "init") != 0) {
		print_plsync_init_only_usage();
		exit(1);
	}
	return argv[1];
}

const char *parse_plsync_args(int argc, char *argv[]) {
	if (argc < 2 || strcmp(argv[1], "-h")==0 || strcmp(argv[1], "--help")==0) {
		print_plsync_usage();
		exit(1);
	}
	for (const char *cmd: COMMANDS) {
		if (strcmp(cmd, argv[1]) == 0) {
			return cmd;
		}
	}
	print_plsync_usage();
	exit(1);
}

int main(int argc, char *argv[]) {
	bool is_yt_init = is_refresh_tkn_valid(Platform::YOUTUBE);
	bool is_sp_init = is_refresh_tkn_valid(Platform::SPOTIFY);
	if (!is_yt_init || !is_sp_init) {
		parse_plsync_args_init_only(argc, argv);
		return run_init(!is_yt_init, !is_sp_init);
	}

	// TODO reduce the number of files by putting all
	// seperate command files into this single file
	const char *command = parse_plsync_args(argc, argv);
	if (strcmp(command, "init") == 0) {
		return run_init(true, true);
	} else if (strcmp(command, "untracked") == 0) {
		return run_untracked(argc-2, argv+2);
	} else if (strcmp(command, "track") == 0) {
		return run_track(argc-2, argv+2);
	} else if (strcmp(command, "tracked") == 0) {
		return run_tracked(argc-2, argv+2);
	} else if (strcmp(command, "untrack") == 0) {
		return run_untrack(argc-2, argv+2);
	} else if (strcmp(command, "sync") == 0) {
		return run_sync(argc-2, argv+2);
	}
	return 0;
}
