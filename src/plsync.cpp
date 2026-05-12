#include "../include/api.h"
#include "../include/cache.h"
#include "../include/new_api.h"
#include "../include/platform.h"
#include "../include/util.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

const std::vector<const char*> COMMANDS = {"init",    "untracked", "track",
                                           "tracked", "untrack",   "sync"};

/* init-start */
const std::string init_description =
    "Allow OAuth permissions for Youtube and Spotify. Initially the only valid "
    "command.";

bool get_user_permissions(Platform plat, std::shared_ptr<CURL> curl) {
    auto api = BaseAuthAPI::get_api(plat, curl);

    // direct user to permission screen
    std::ostringstream cmd;
    cmd << "open '" << api->get_auth_url() << "'";
    system(cmd.str().c_str());

    // listen at redirect url for auth_code
    std::string auth_code;
    std::cout << "Waiting for " << platform_title(plat)
              << " authentication code... " << std::flush;
    if (!api->collect_auth_code()) {
        std::cout << "Unable to complete " << platform_title(plat)
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
    save_access_tkn(plat, tkn_resp.access_tkn, tkn_resp.access_duration);
    save_refresh_tkn(plat, tkn_resp.refresh_tkn);
    std::cout << "Success! " << platform_title(plat)
              << " authentication completed" << '\n';
    return true;
}

int run_init(bool init_youtube, bool init_spotify) {
    auto curl = get_curl();
    try {
        if (init_youtube) {
            get_user_permissions(Platform::YOUTUBE, curl);
        }
        if (init_spotify) {
            get_user_permissions(Platform::SPOTIFY, curl);
        }
    } catch (const API::RequestError& e) {
        std::cerr << "Something went wrong. Please try again\n";
        return 1;
    } catch (const TokenStorageAccessError& e) {
        std::cerr << "Something went wrong. Please try again\n";
        return 1;
    }
    return 0;
}

/* untracked-start */
// TODO --force option because a Playlist::num_items can go stale if Playlist is
// tracked and untracked before every being synced which user may notice
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

int untracked(int argc, char* argv[]) {
    Platform plat = parse_untracked_args(argc, argv);
    std::string tkn;
    std::shared_ptr<CURL> curl = get_curl();

    get_or_fetch_access_tkn(plat, curl, tkn);
    std::unique_ptr<BaseDataAPI> api = BaseDataAPI::get_api(plat, curl, tkn);

    PlaylistCache::Handle cache(plat);
    std::vector<Playlist> modified_playlists;
    std::string modified_etag = cache.head->etag;
    bool modified = api->get_playlists(modified_playlists, modified_etag);
    if (modified) {
        PlaylistCache::update(cache.head, cache.plat, modified_playlists,
                              modified_etag);
    }
    PlaylistCache::save(cache.head, cache.plat);

    size_t longest_title = 0;
    for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
        longest_title = std::max(longest_title, utf8_len(it->title));
    }

    int sid_len = PlaylistCache::short_id_len(plat);
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
        if (!it.ptr.node->playlist.tracker.empty()) {
            continue;
        }
        std::string sid = bin_to_hex(it->id_hash.substr(0, sid_len));
        int title_pad =
            std::max(longest_title, size_t(5)) + 1 - utf8_len(it->title);
        std::string privacy_type = it->is_private ? "private" : "public";
        int privacy_pad = it->is_private ? 1 : 2;
        std::cout << std::setw(id_wd) << sid << std::setw(0) << it->title
                  << std::string(title_pad, ' ') << std::setw(privacy_wd)
                  << privacy_type << it->num_items << '\n';
    }
    return 0;
}

int run_untracked(int argc, char* argv[]) {
    try {
        return untracked(argc, argv);
    } catch (const TokenStorageAccessError& e) {
        std::cerr << "Couldn't get access token. Please try again\n";
    } catch (const BaseAPI::RequestError& e) {
        std::cerr << "Something went wrong. Try again.\n";
    }
    return 1;
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

int track(int argc, char* argv[]) {
    std::unordered_map<Platform, std::string> plat_to_sid =
        parse_track_args(argc, argv);
    std::shared_ptr<CURL> curl = get_curl();
    std::vector<PlaylistCache::Node> nodes;
    std::string tracker_id;
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

        // check at most one playlist is already tracked
        if (!node.playlist.tracker.empty()) {
            if (!tracker_id.empty()) {
                throw std::invalid_argument("");
            } else {
                tracker_id = node.playlist.tracker;
            }
        }
        if (playlist_title.empty()) {
            playlist_title = node.playlist.title;
        }
        nodes.push_back(std::move(node));
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
        nodes.push_back(std::move(node));
    }

    // and finally track the provided playlists
    auto tracker =
        (!tracker_id.empty()) ? PlaylistTracker(tracker_id) : PlaylistTracker();
    for (PlaylistCache::Node& n : nodes) {
        if (!n.playlist.tracker.empty()) {
            continue;
        }
        n.playlist.tracker = tracker.id;
        // num_items now represents num of items stored locally in
        // PlaylistItemsCache, not num of items in actual Playlist served by API
        n.playlist.num_items = 0;
        n.was_changed = true;
        tracker.nodes.push_back(std::move(n));
        tracker.was_changed = true;
    }
    tracker.save();
    return 0;
}

int run_track(int argc, char* argv[]) {
    try {
        return track(argc, argv);
    } catch (const std::invalid_argument& e) {
        print_track_usage();
    } catch (const TokenStorageAccessError& e) {
        std::cout << "Something went wrong. Please try again\n";
    } catch (const API::RequestError& e) {
        std::cout << "Something went wrong. Please try again\n";
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

int run_tracked(int argc, char* argv[]) {
    if (!parse_tracked_args(argc, argv)) {
        return 0;
    }

    PlaylistItemsCache cache;
    size_t longest_title = 0;
    for (const PlaylistTracker& t : cache.trackers) {
        for (const PlaylistCache::Node& n : t.nodes) {
            longest_title = std::max(longest_title, utf8_len(n.playlist.title));
        }
    }

    std::vector<std::size_t> plat_to_sid_len(Platform::INVALID, 0);
    int longest_sid = 0;
    for (int i = 0; i != Platform::INVALID; i++) {
        Platform plat = static_cast<Platform>(i);
        int sid_len = PlaylistCache::short_id_len(plat);
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
    for (const PlaylistTracker& t : cache.trackers) {
        if (skip_newline) {
            skip_newline = false;
        } else {
            std::cout << '\n';
        }
        for (const PlaylistCache::Node& node : t.nodes) {
            int sid_len = plat_to_sid_len[node.playlist.plat];
            std::string sid = node.playlist.id_hash.substr(0, sid_len);
            std::cout << std::setw(plat_wd)
                      << platform_title(node.playlist.plat) << std::setw(id_wd)
                      << bin_to_hex(sid) << node.playlist.title << '\n';
        }
        std::cout << t.nodes[0].playlist.num_items << " song(s)\n";
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
                 "Can either be 'yt' or a prefix of 'youtube' and spotify\n"
                 "  playlist-id  Value of the id field shown in the output of "
                 "<tracked> for the playlist to untrack\n";
}

PlaylistCache::Node parse_untrack_args(int argc, char* argv[]) {
    if (argc != 2) {
        throw std::invalid_argument("");
    }
    if (parse_platform(argv[0]) == Platform::INVALID) {
        throw std::invalid_argument("");
    }
    if (strlen(argv[1]) % 2 != 0) {
        throw std::invalid_argument("");
    }

    Platform plat = parse_platform(argv[0]);
    PlaylistCache::Node node =
        PlaylistCache::load_node_sid(hex_to_bin(argv[1]), plat);
    if (node.playlist.tracker.empty()) {
        throw std::invalid_argument("");
    }
    return node;
}

int untrack(int argc, char* argv[]) {
    PlaylistCache::Node node = parse_untrack_args(argc, argv);
    PlaylistTracker tracker(node.playlist.tracker);
    tracker.untrack(node.playlist.plat);
    tracker.save();
    return 0;
}

int run_untrack(int argc, char* argv[]) {
    try {
        return untrack(argc, argv);
    } catch (const std::invalid_argument& e) {
        print_untrack_usage();
    }
    return 1;
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

void update_tracked_playlists(PlaylistItemsCache& cache,
                              std::shared_ptr<CURL> curl,
                              std::vector<std::string>& plat_to_tkn) {
    auto prev = cache.trackers.before_begin();
    auto curr = cache.trackers.begin();
    while (curr != cache.trackers.end()) {
        int i = 0;
        while (i < curr->nodes.size()) {
            PlaylistCache::Node& node = curr->nodes[i];
            Playlist& pl = node.playlist;
            Playlist mod_pl;
            bool is_mod =
                API::get_playlist(pl.plat, curl.get(), plat_to_tkn[pl.plat],
                                  pl.id, pl.etag, mod_pl);
            if (is_mod && mod_pl.id.empty()) {
                PlaylistCache::Node copy(node);
                curr->untrack(pl.plat);
                PlaylistCache::remove_node(copy, copy.playlist.plat);
                continue;
            }
            if (is_mod) {
                int num_items = pl.num_items;
                pl.merge(std::move(mod_pl));
                pl.num_items = num_items;
                node.was_changed = true;
            }
            i++;
        }

        if (curr->nodes.empty()) {
            curr = cache.trackers.erase_after(prev);
        } else {
            ++prev;
            ++curr;
        }
    }
}

int sync(PlaylistItemsCache& cache) {
    std::cout << "Syncing... " << std::flush;
    std::shared_ptr<CURL> curl = get_curl();
    std::vector<std::string> plat_to_access_tkn = get_access_tokens(curl);
    update_tracked_playlists(cache, curl, plat_to_access_tkn);

    for (PlaylistTracker& tracker : cache.trackers) {
        std::vector<PlaylistDiff> diffs;
        PlaylistDiff net_diff;
        for (PlaylistCache::Node& n : tracker.nodes) {
            Playlist& pl = n.playlist;
            SongCounts song_cnts;
            for (const auto& [song, items] : pl.items.data) {
                song_cnts[song] = items.size();
            }

            bool modified = API::get_playlist_items(
                pl.plat, curl.get(), plat_to_access_tkn[pl.plat],
                plat_to_access_tkn[Platform::SPOTIFY], pl);
            if (modified) {
                pl.items.was_changed = true;
                SongCounts mod_song_cnts;
                for (const auto& [song, items] : pl.items.data) {
                    mod_song_cnts[song] = items.size();
                }

                PlaylistDiff diff = mod_song_cnts - song_cnts;
                net_diff += diff;
                diffs.push_back(std::move(diff));
            } else {
                diffs.emplace_back();
            }
        }

        if (net_diff.added.empty() && net_diff.removed.empty()) {
            continue;
        }

        for (int i = 0; i != tracker.nodes.size(); i++) {
            Playlist& pl = tracker.nodes[i].playlist;
            PlaylistDiff diff = net_diff - diffs[i];
            API::playlist_items_add(pl.plat, curl.get(),
                                    plat_to_access_tkn[pl.plat], pl,
                                    diff.added);
            API::playlist_items_remove(pl.plat, curl.get(),
                                       plat_to_access_tkn[pl.plat], pl,
                                       diff.removed);
            // if one PlaylistItems has changed, then all others tracking it
            // must change too
            pl.items.was_changed = true;
        }

        int num_items = 0;
        for (const auto& [_, items] : tracker.nodes[0].playlist.items.data) {
            num_items += items.size();
        }
        for (PlaylistCache::Node& n : tracker.nodes) {
            n.playlist.num_items = num_items;
            n.playlist.was_changed = true;
        }
    }
    cache.save();
    std::cout << "Done!\n";
    return 0;
}

int run_sync(int argc, char* argv[]) {
    if (!parse_sync_args(argc, argv)) {
        return 0;
    }
    PlaylistItemsCache cache;
    try {
        return sync(cache);
    } catch (const TokenStorageAccessError& e) {
        cache.save();
    } catch (const API::RequestError& e) {
        cache.save();
    }
    return 1;
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
    } else {
        print_plsync_usage();
    }
    return 0;
}
