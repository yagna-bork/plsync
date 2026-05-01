#include "../include/api.h"
#include "../include/cache.h"
#include "../include/cache.pb.h"
#include "../include/new_api.h"
#include "../include/platform.h"
#include "../include/util.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <ios>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

const std::vector<const char*> COMMANDS = {"init",    "untracked", "track",
                                           "tracked", "untrack",   "sync"};

/* init-start */
const std::string init_description =
    "Allow OAuth permissions for Youtube and Spotify. Initially the only valid "
    "command.";

bool get_user_permissions(Platform platform, std::shared_ptr<CURL> curl) {
    auto api = BaseAuthAPI::get_api(platform, curl);

    // direct user to permission screen
    std::ostringstream cmd;
    cmd << "open '" << api->get_auth_url() << "'";
    system(cmd.str().c_str());

    // listen at redirect url for auth_code
    std::string auth_code;
    std::cout << "Waiting for " << platform_title(platform)
              << " authentication code... " << std::flush;
    if (!api->collect_auth_code()) {
        std::cout << "Unable to complete " << platform_title(platform)
                  << " authentication. Please try again" << '\n';
        return false;
    }
    std::cout << "Got it!\n";

    // exchange auth_code for access & refresh tokens
    BaseAuthAPI::TokenResponse tkn_resp;
    try {
        tkn_resp = api->exchange_auth_code();
    } catch (const BaseAuthAPI::RequestError& e) {
        std::cerr << "Something went wrong. Please try again\n";
        return false;
    }

    // finally store the tokens
    if (!save_access_tkn(platform, tkn_resp.access_tkn,
                         tkn_resp.access_duration)) {
        std::cerr << "Couldn't store tokens in keychain. Please try again"
                  << '\n';
        return false;
    }
    if (!save_refresh_tkn(platform, tkn_resp.refresh_tkn)) {
        std::cerr << "Couldn't store tokens in keychain. Please try again"
                  << '\n';
        return false;
    }
    std::cout << "Success! " << platform_title(platform)
              << " authentication completed" << '\n';
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
const std::string untracked_description =
    "Display information about playlists which are not being tracked";

void print_untracked_usage() {
    std::cout << "usage: plsync untracked <platform>\n\n"
              << untracked_description << "\n\n"
              << "Options:\n"
              << "  platform  Name of platform to view playlists from. Can "
                 "either be 'yt' or a prefix of 'youtube' and 'spotify'\n";
}

Platform parse_untracked_args(int argc, char* argv[]) {
    if (argc != 1 || strcmp(argv[0], "-h") == 0 ||
        strcmp(argv[0], "--help") == 0) {
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

int run_untracked(int argc, char* argv[]) {
    Platform platform = parse_untracked_args(argc, argv);
    std::string tkn;
    std::shared_ptr<CURL> curl = get_curl();
    if (!get_or_fetch_access_tkn(platform, curl, tkn)) {
        std::cerr << "Couldn't get access token. Please try again\n";
        return 1;
    }
    std::unique_ptr<BaseDataAPI> api =
        BaseDataAPI::get_api(platform, curl, tkn);

    PlaylistCache::Handle cache(platform);
    std::vector<Playlist> modified_playlists;
    std::string modified_etag = cache.head->etag;
    bool modified;
    try {
        modified = api->get_playlists(modified_playlists, modified_etag);
    } catch (const BaseAPI::RequestError& e) {
        std::cerr << "Something went wrong. Try again.\n";
        return 1;
    }
    if (modified) {
        PlaylistCache::update(cache.head, cache.plat, modified_playlists,
                              modified_etag);
    }
    PlaylistCache::save(cache.head, cache.plat);

    size_t longest_title = 0;
    for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
        longest_title = std::max(longest_title, utf8_len(it->title));
    }

    int sid_len = playlist_tree_height(platform);
    int id_wd = std::max(2, sid_len * 2) + 1;
    int privacy_wd = strlen("private") + 1;

    std::stringstream heading_ss;
    heading_ss << std::left << std::setw(id_wd) << "Id"
               << std::setw(longest_title + 1) << "Title"
               << std::setw(privacy_wd) << "Privacy" << std::setw(0) << "Items";
    std::string heading = heading_ss.str();
    std::cout << heading << '\n';
    std::cout << std::string(heading.size(), '-') << '\n';

    std::cout << std::left;
    // TODO sort
    for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
        if (!it.ptr.node->items_id.empty()) {
            continue;
        }
        std::string sid = bin_to_hex(it->id_hash.substr(0, sid_len));
        int title_pad =
            std::max(longest_title, size_t(5)) + 1 - utf8_len(it->title);
        std::string privacy_type = it->is_private ? "private" : "public";
        int privacy_pad = it->is_private ? 1 : 2;
        std::cout << std::setw(id_wd) << sid << std::setw(0) << it->title
                  << std::string(title_pad, ' ') << std::setw(privacy_wd)
                  << privacy_type << it->items << '\n';
    }
    return 0;
}

/* track-start */
const std::string track_description = "Start tracking an untracked playlist";

void print_track_usage() {
    std::cout << "usage: plsync track <platform> <playlist-id>\n\n"
              << track_description << "\n\n"
              << "Options:\n"
              << "  platform     Name of platform to view playlists from. Can "
                 "either be 'yt' or a prefix of 'youtube' and 'spotify'\n"
              << "  playlist-id  Value of the id field shown in the output of "
                 "<untracked> for the playlist to track\n";
}

std::unordered_map<Platform, std::string> parse_track_args(int argc,
                                                           char* argv[]) {
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
        if (prev_plat == Platform::INVALID || !plat_to_sid[prev_plat].empty() ||
            std::strlen(argv[i]) % 2 != 0) {
            throw std::invalid_argument("");
        }
        plat_to_sid[prev_plat] = argv[i];
    }

    auto sid_empty = [](const std::pair<Platform, std::string>& pair) {
        return pair.second.empty();
    };
    if (plat_to_sid.size() < 2 ||
        std::all_of(plat_to_sid.begin(), plat_to_sid.end(), sid_empty)) {
        throw std::invalid_argument("");
    }
    return plat_to_sid;
}

void track(int argc, char* argv[]) {
    std::unordered_map<Platform, std::string> plat_to_sid =
        parse_track_args(argc, argv);
    std::shared_ptr<CURL> curl = get_curl();
    std::unordered_map<Platform, PlaylistCache::Node> plat_to_node;
    std::string items_id;
    std::string playlist_title;
    for (const auto& pair : plat_to_sid) {
        auto plat = pair.first;
        const auto& sid = pair.second;
        if (sid.empty()) {
            continue;
        }

        // check sid is valid
        PlaylistCache::Node node =
            PlaylistCache::load_node_sid(hex_to_bin(sid), plat);
        if (node.playlist.id.empty()) {
            throw std::invalid_argument("");
        }

        // check playlist wasn't deleted
        std::string access_tkn = get_or_refresh_access_tkn(plat, curl);
        Playlist modified_playlist;
        bool modified =
            API::get_playlist(plat, curl.get(), access_tkn, node.playlist.id,
                              node.playlist.etag, modified_playlist);
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
    for (const auto& pair : plat_to_sid) {
        if (!pair.second.empty()) {
            continue;
        }
        Platform plat = pair.first;
        std::string access_tkn = get_or_refresh_access_tkn(plat, curl);
        Playlist playlist =
            API::create_playlist(plat, curl.get(), access_tkn, playlist_title);
        PlaylistCache::Node node(playlist);
        create_node(node, plat);
        plat_to_node[plat] = std::move(node);
    }

    // and finally track the provided playlists
    for (auto& pair : plat_to_node) {
        const Platform& plat = pair.first;
        PlaylistCache::Node& node = pair.second;
        if (!node.items_id.empty()) {
            continue;
        }
        pl_items.tracked.emplace_back(plat, Playlist(node.playlist.id,
                                                     node.playlist.id_hash,
                                                     /*items_etag=*/""));
        node.items_id = pl_items.id;
        save_node(node, plat);
    }
    save_playlist_items(pl_items);
}

int run_track(int argc, char* argv[]) {
    try {
        track(argc, argv);
        return 0;
    } catch (const std::invalid_argument& e) {
        print_track_usage();
    } catch (const std::exception& e) {
        std::cerr << "Something went wrong please try again\n";
    }
    return 1;
}

/* tracked-start */
const std::string tracked_description =
    "Display information about which playlists are being tracked";

void print_tracked_usage() {
    std::cout << "usage: plsync tracked\n\n" << tracked_description << '\n';
}

bool parse_tracked_args(int argc, char* argv[]) {
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

    std::shared_ptr<CURL> curl = get_curl();
    PlaylistItemsCache cache = load_playlist_items_cache();
    try {
        update_playlist_items_cache(cache, curl, get_access_tokens(curl));
    } catch (const TokenStorageAccessError& e) {
        return save_cache_quit(cache);
    } catch (const API::RequestError& e) {
        return save_cache_quit(cache);
    }
    // could've used RAII but not worth the effort for such simple case
    save_playlist_items_cache(cache);

    size_t longest_title = 0;
    auto it = cache.cbegin();
    for (const PlaylistItems& pl_items : cache) {
        for (const auto& pair : pl_items.tracked) {
            const Playlist& pl = pair.second;
            longest_title = std::max(longest_title, pl.title.size());
        }
    }

    std::vector<std::size_t> plat_to_sid_len(Platform::INVALID, 0);
    int longest_sid = 0;
    for (int i = 0; i != Platform::INVALID; i++) {
        Platform plat = static_cast<Platform>(i);
        int sid_len = playlist_tree_height(plat);
        plat_to_sid_len[plat] = sid_len;
        longest_sid = std::max(longest_sid, sid_len);
    }

    int id_wd = std::max(2, longest_sid * 2) + 1;
    int plat_wd = strlen("Platform") + 1;

    std::ostringstream heading_ss;
    heading_ss << std::left << std::setw(plat_wd) << "Platform"
               << std::setw(id_wd) << "Id" << std::setw(longest_title)
               << "Title";
    std::string heading = heading_ss.str();
    std::cout << std::left << heading << '\n'
              << std::string(heading.size(), '-') << '\n';

    bool skip_newline = true;
    for (const auto& pl_items : cache) {
        if (skip_newline) {
            skip_newline = false;
        } else {
            std::cout << '\n';
        }
        for (const auto& pair : pl_items.tracked) {
            const Platform& plat = pair.first;
            const Playlist& pl = pair.second;
            std::string sid = pl.id_hash.substr(0, plat_to_sid_len[plat]);
            std::cout << std::setw(plat_wd) << platform_title(plat)
                      << std::setw(id_wd) << bin_to_hex(sid) << pl.title
                      << '\n';
        }
        size_t num_songs = 0;
        for (const auto& [_, cnt] : pl_items.song_counts) {
            num_songs += cnt;
        }
        std::cout << num_songs << " song(s)\n";
    }
    return 0;
}

/* untrack-start */
const std::string untrack_description = "Stop tracking a tracked playlist";

void print_untrack_usage() {
    std::cout << "usage: plsync untrack <platform> <playlist-id>\n\n"
              << untrack_description << "\n\n"
              << "Options:\n"
              << "  platform     The platform that the playlist belongs to. "
                 "Can either be 'yt' or a prefix of 'youtube' and 'spotify'\n"
              << "  playlist-id  Value of the id field shown in the output of "
                 "<tracked> for the playlist to untrack\n";
}

void parse_untrack_args(int argc, char* argv[], Platform& plat,
                        PlaylistCache::Node& node) {
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
    node = PlaylistCache::load_node_sid(hex_to_bin(argv[1]), plat);
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
        for (auto it = pl_items.tracked.begin(); it != pl_items.tracked.end();
             it++) {
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
    }
}

/* sync-start */
const std::string sync_description = "Sync your tracked playlists";

void print_sync_usage() {
    std::cout << "usage: plsync sync\n\n" << sync_description << '\n';
}

bool parse_sync_args(int argc, char* argv[]) {
    if (argc > 0) {
        print_sync_usage();
        return false;
    }
    return true;
}

int sync(PlaylistItemsCache& cache) {
    std::cout << "Syncing... " << std::flush;
    std::shared_ptr<CURL> curl = get_curl();
    std::vector<std::string> plat_to_access_tkn = get_access_tokens(curl);
    update_playlist_items_cache(cache, curl, plat_to_access_tkn);

    for (PlaylistItems& pl_items : cache) {
        // precompute the hashes for performance because a lot of
        // comparisons will be made by the diffing algorithm
        SongHashCounts songh_counts;
        std::hash<Song> hasher;
        for (const auto& [song, count] : pl_items.song_counts) {
            songh_counts[hasher(song)] = count;
        }

        std::vector<PlaylistDiff> changes;
        std::unordered_map<size_t, Song> songh_to_song;
        PlaylistDiff net_change;
        for (std::pair<Platform, Playlist>& pair : pl_items.tracked) {
            SongCounts mod_song_counts;
            bool modified = API::get_song_counts(
                pair.first, curl.get(), plat_to_access_tkn[pair.first],
                plat_to_access_tkn[Platform::SPOTIFY], pair.second.id,
                mod_song_counts, pair.second.items_etag);

            if (modified) {
                SongHashCounts mod_songh_counts;
                for (const auto& [song, count] : mod_song_counts) {
                    size_t hash = hasher(song);
                    mod_songh_counts[hash] = count;
                    if (!songh_to_song.count(hash)) {
                        songh_to_song[hash] = song;
                    }
                }

                PlaylistDiff change = mod_songh_counts - songh_counts;
                net_change += change;
                changes.push_back(std::move(change));
            } else {
                changes.emplace_back();
            }
        }

        for (int i = 0; i != pl_items.tracked.size(); i++) {
            PlaylistDiff to_change = net_change - changes[i];
            // API::update_playlist(pl_items.tracked[i].plat,
            // pl_items.tracked[i].id, to_change);
        }
    }
    std::cout << "Done!\n";
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
void print_plsync_init_only_usage() {
    std::cout << "usage: plsync [-h] <command>\n\n"
              << "Avaliable commands:\n"
              << "  init  Allow OAuth permissions for Youtube and Spotify. "
                 "Initially the only valid command"
              << std::endl;
}

void print_plsync_usage() {
    std::cout << "usage: plsync [-h] <command>\n\n"
              << "Avaliable commands:\n"
              << "  init       " << init_description << "\n\n"
              << "  untracked  " << untracked_description << "\n\n"
              << "  track      " << track_description << "\n\n"
              << "  tracked    " << tracked_description << "\n\n"
              << "  untrack    " << untrack_description << "\n\n"
              << "  sync       " << sync_description << '\n';
}

const char* parse_plsync_args_init_only(int argc, char* argv[]) {
    if (argc < 2 || strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "--help") == 0) {
        print_plsync_init_only_usage();
        exit(1);
    }
    if (strcmp(argv[1], "init") != 0) {
        print_plsync_init_only_usage();
        exit(1);
    }
    return argv[1];
}

const char* parse_plsync_args(int argc, char* argv[]) {
    if (argc < 2 || strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "--help") == 0) {
        print_plsync_usage();
        exit(1);
    }
    for (const char* cmd : COMMANDS) {
        if (strcmp(cmd, argv[1]) == 0) {
            return cmd;
        }
    }
    print_plsync_usage();
    exit(1);
}

int main(int argc, char* argv[]) {
    bool is_yt_init = is_refresh_tkn_valid(Platform::YOUTUBE);
    bool is_sp_init = is_refresh_tkn_valid(Platform::SPOTIFY);
    if (!is_yt_init || !is_sp_init) {
        parse_plsync_args_init_only(argc, argv);
        return run_init(!is_yt_init, !is_sp_init);
    }

    // TODO reduce the number of files by putting all
    // seperate command files into this single file
    const char* command = parse_plsync_args(argc, argv);
    if (strcmp(command, "init") == 0) {
        return run_init(true, true);
    } else if (strcmp(command, "untracked") == 0) {
        return run_untracked(argc - 2, argv + 2);
    } else if (strcmp(command, "track") == 0) {
        return run_track(argc - 2, argv + 2);
    } else if (strcmp(command, "tracked") == 0) {
        return run_tracked(argc - 2, argv + 2);
    } else if (strcmp(command, "untrack") == 0) {
        return run_untrack(argc - 2, argv + 2);
    } else if (strcmp(command, "sync") == 0) {
        return run_sync(argc - 2, argv + 2);
    }
    return 0;
}
