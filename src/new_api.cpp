#include "../include/cache.h"
#include "../include/new_api.h"
#include "../include/platform.h"
#include "../include/util.h"
#include <algorithm>
#include <cstddef>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace nlohmann;

namespace API {

static std::string fields_to_string(const Fields& fields) {
    std::string fields_str;
    for (int i = 0; i != fields.size(); i++) {
        const auto& field = fields[i];
        if (i > 0) {
            fields_str.push_back('&');
        }
        fields_str.append(field.first);
        fields_str.push_back('=');
        fields_str.append(field.second);
    }
    return fields_str;
}

static std::string append_params(const std::string& url, const Params& params) {
    std::ostringstream ss(url, std::ios::ate);
    for (std::size_t i = 0; i != params.size(); i++) {
        char sep = (i == 0) ? '?' : '&';
        ss << sep << params[i].first << '=' << params[i].second;
    }
    return ss.str();
}

static std::string decompress_gzip(std::filesystem::path file) {
    gzFile_s* gzf = gzopen(file.c_str(), "rb");
    if (!gzf) {
        gzclose(gzf);
        throw RequestError("couldn't open gzip file");
    }

    std::string decompressed;
    size_t bufsz = 512;
    std::string buf(bufsz, 0);
    int nread;
    while ((nread = gzread(gzf, buf.data(), bufsz)) > 0) {
        std::copy(buf.begin(), buf.begin() + nread,
                  std::back_inserter(decompressed));
    }
    gzclose(gzf);
    if (nread == -1) {
        throw RequestError("couldn't decompress response");
    }
    return decompressed;
}

static long status_code(CURL* curl) {
    long status_code;
    if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code) !=
        CURLE_OK) {
        throw RequestError("couldn't retrieve http status code");
    }
    return status_code;
}

static std::string get_song_hash(const Song& song) {
    std::ostringstream str;
    for (int i = 0; i != song.artists.size(); i++) {
        if (i > 0) {
            str << ',';
        }
        str << song.artists[i];
    }
    str << ':' << song.track;
    return sha256(str.str());
}

long GET(CURL* curl, const std::string& url, json& jresp, const Params& params,
         const std::string& access_tkn, const std::string& etag) {
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, append_params(url, params).c_str());

    curl_slist_raii headers;
    headers.append("Accept: application/json");
    headers.append("If-None-Match: " + etag);
    headers.append("Accept-Encoding: gzip");
    headers.append("User-Agent: plsync (gzip)");
    if (!access_tkn.empty()) {
        headers.append("Authorization: Bearer " + access_tkn);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());

    /* store response in a .gz (gzip) file */
    std::filesystem::path tmpdir;
    if (!ensure_tmpdir(tmpdir)) {
        throw RequestError("couldn't access temp directory");
    }
    std::filesystem::path resp_path =
        tmpdir / ("resp." + urlencode64(rndstr(8)) + ".gz");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite_cb);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    // wrapped file stream in scope so its guaranteed
    // to be flushed before decompression
    {
        std::ofstream respf(resp_path, std::ios::binary);
        if (!respf) {
            throw RequestError("couldn't create response file");
        }
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respf);
        if (curl_easy_perform(curl) != CURLE_OK) {
            throw RequestError("curl request failed");
        }
    }

    std::string resp = decompress_gzip(resp_path);
    jresp = json::parse(resp.empty() ? "{}" : resp);
    return status_code(curl);
}

long POST(CURL* curl, const std::string& url, const std::string& data,
          json& jresp, const std::string& application_type,
          const Params& params, const std::string& access_tkn) {
    curl_easy_reset(curl);

    std::string resp;
    curl_easy_setopt(curl, CURLOPT_URL, append_params(url, params).c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

    curl_slist_raii headers;
    headers.append("Accept: application/json");
    if (!application_type.empty()) {
        headers.append("Content-Type: " + application_type);
    }
    if (!access_tkn.empty()) {
        headers.append("Authorization: Bearer " + access_tkn);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());

    curl_easy_perform(curl);
    jresp = json::parse(resp.empty() ? "{}" : resp);
    return status_code(curl);
}

long DELETE(CURL* curl, const std::string& url, nlohmann::json& jresp,
            const std::string& access_tkn, const Params& params,
            const std::string& data) {
    curl_easy_reset(curl);

    std::string resp;
    curl_easy_setopt(curl, CURLOPT_URL, append_params(url, params).c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    if (!data.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    }

    curl_slist_raii headers;
    headers.append("Accept: application/json");
    headers.append("Authorization: Bearer " + access_tkn);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());

    curl_easy_perform(curl);
    jresp = json::parse(resp.empty() ? "{}" : resp);
    return status_code(curl);
}

} // namespace API

using namespace API;

namespace NewYoutubeAPI {

long _GET_paginated(CURL* curl, const std::string& url, json& resp,
                    Params& params, const std::string& access_tkn = "") {
    json next_page;
    long status_code = GET(curl, url, next_page, params, access_tkn);
    if (status_code != 200L) {
        return status_code;
    }
    std::move(next_page["items"].begin(), next_page["items"].end(),
              std::back_inserter(resp["items"]));

    if (next_page.contains("nextPageToken")) {
        params.back().second = next_page["nextPageToken"];
        return _GET_paginated(curl, url, resp, params, access_tkn);
    } else {
        return status_code;
    }
}

static long GET_paginated(CURL* curl, const std::string& url, json& resp,
                          Params& params, const std::string& access_tkn = "",
                          const std::string& etag = "") {
    long status_code = GET(curl, url, resp, params, access_tkn, etag);
    if (status_code != 200L) {
        return status_code;
    }
    if (resp.contains("nextPageToken")) {
        params.emplace_back("pageToken", resp["nextPageToken"]);
        return _GET_paginated(curl, url, resp, params, access_tkn);
    } else {
        return status_code;
    }
}

bool get_playlist(CURL* curl, const std::string& access_tkn,
                  const std::string& id, const std::string& etag,
                  Playlist& res) {
    std::string url = base_url + "/playlists";
    Params params = {
        {"id", id},
        {"part", "id,snippet,status,contentDetails"},
        {"fields",
         "etag,items("
         "id,etag,snippet/title,status/privacyStatus,contentDetails/itemCount"
         ")"},
    };
    json resp;
    long status_code = GET(curl, url, resp, params, access_tkn, etag);

    if (status_code == 304L) {
        return false;
    } else if (status_code == 200L) {
        if (resp["items"].size() == 0) {
            res = Playlist();
        } else {
            res = Playlist(
                resp["items"][0]["id"], resp["etag"], resp["items"][0]["etag"],
                resp["items"][0]["snippet"]["title"], Platform::YOUTUBE,
                (resp["items"][0]["status"]["privacyStatus"] == "private"),
                resp["items"][0]["contentDetails"]["itemCount"]);
        }
        return true;
    } else {
        throw RequestError("Invalid response from youtube");
    }
}

Playlist create_playlist(CURL* curl, const std::string& access_tkn,
                         const std::string& title) {
    std::string url = base_url + "/playlists";
    Params params = {{"part", "id,snippet,status,contentDetails"}};
    json data;
    data["snippet"]["title"] = title;
    data["snippet"]["description"] = "Created by plsync";
    data["status"]["privacyStatus"] = "private";

    json resp;
    long status_code = POST(curl, url, data.dump(), resp, "application/json",
                            params, access_tkn);
    if (status_code != 200) {
        throw RequestError("Invalid response from google");
    }
    return Playlist(std::move(resp["id"]), "", std::move(resp["etag"]),
                    std::move(resp["snippet"]["title"]), Platform::YOUTUBE,
                    resp["status"]["privacyStatus"] == "private",
                    resp["contentDetails"]["itemCount"]);
}

static bool parse_song_from_html(CURL* curl, const std::string& video_id,
                                 Song& out_song) {
    std::string url = "https://www.youtube.com/watch?v=" + video_id;
    std::string html_data;
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    curl_slist_raii headers;
    headers.append("Accept: text/html");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    if (curl_easy_perform(curl) != CURLE_OK) {
        throw RequestError("curl request failed");
    }

    size_t attrs_beg = html_data.rfind("videoAttributeViewModel");
    if (attrs_beg == html_data.npos) {
        return false;
    }
    attrs_beg -= 2; // retreat to start of json object
    size_t i = attrs_beg;
    size_t num_open_brackets = 0;
    while (i == attrs_beg || num_open_brackets) {
        if (html_data[i] == '{') {
            num_open_brackets++;
        } else if (html_data[i] == '}') {
            num_open_brackets--;
        }
        i++;
    }
    json attrs = json::parse(
        std::string(html_data.begin() + attrs_beg, html_data.begin() + i));
    out_song.artists.push_back(
        std::move(attrs["videoAttributeViewModel"]["subtitle"]));
    out_song.track = std::move(attrs["videoAttributeViewModel"]["title"]);
    return true;
}

static bool parse_song_from_title(const std::string& title, Song& out_song) {
    std::regex re("^([\\w][\\w ]*(, [\\w][\\w ]*)*) - ([\\w][\\w ]*)"
                  "( \\(?(ft\\.|feat\\.|featuring|with|w/) ([\\w][\\w ]*(, "
                  "[\\w][\\w ]*)*)\\)?)?$");
    std::smatch m;
    if (!std::regex_match(title, m, re)) {
        return false;
    }

    out_song.track = std::string(m[3].first, m[3].second);
    out_song.artists = split(std::string(m[1].first, m[1].second), ", ");
    if (m[6].matched) {
        std::vector<std::string> features =
            split(std::string(m[6].first, m[6].second), ", ");
        std::move(features.begin(), features.end(),
                  std::back_inserter(out_song.artists));
    }
    return true;
}

static inline std::string get_item(const std::string& item_id,
                                   const std::string& vid_id) {
    std::ostringstream res;
    res << item_id << ':' << vid_id;
    return res.str();
}

bool get_playlist_items(CURL* curl, const std::string& yt_access_tkn,
                        const std::string& sp_access_tkn, Playlist& out_pl) {
    std::string url = base_url + "/playlistItems";
    Params params = {{"part", "id,snippet,contentDetails"},
                     {"playlistId", out_pl.id},
                     {"maxResults", "50"}};
    json resp;
    long status_code = GET_paginated(curl, url, resp, params, yt_access_tkn,
                                     out_pl.items.etag);

    if (status_code == 304L) {
        return false;
    } else if (status_code != 200L) {
        throw RequestError("Invalid response from google");
    }
    out_pl.items.data.clear();
    out_pl.items.etag = resp["etag"];
    json& items = resp["items"];
    SongCache cache(Platform::YOUTUBE);

    std::vector<json*> new_items;
    std::unordered_set<std::string> new_vid_ids;
    for (json& item : items) {
        const std::string& iid = item["id"].get_ref<std::string&>();
        const std::string& vid =
            item["contentDetails"]["videoId"].get_ref<std::string&>();
        if (cache.songs.count(vid)) {
            Song& song = cache.songs[vid];
            out_pl.items.data[song].push_back(get_item(iid, vid));
        } else {
            new_items.push_back(&item);
            new_vid_ids.insert(vid);
        }
    }

    int n = new_vid_ids.size();
    std::unordered_map<std::string, std::string> vid_id_to_category_id;
    url = "https://www.googleapis.com/youtube/v3/videos";
    params = {{"part", "id,snippet"}, {"maxResults", "50"}, {"id", ""}};

    auto it = new_vid_ids.cbegin();
    for (int i = 0; i < n; i += 50) {
        std::ostringstream vids;
        for (int j = 0; (i + j != n) && (j != 50); j++) {
            if (j > 0) {
                vids << ',';
            }
            vids << *it++;
        }
        params.back().second = vids.str();
        json videos_resp;
        status_code = GET(curl, url, videos_resp, params, yt_access_tkn);
        const json& videos = videos_resp["items"];

        for (const json& vid : videos) {
            vid_id_to_category_id[vid["id"]] = vid["snippet"]["categoryId"];
        }
    }

    for (json* item : new_items) {
        std::string vid_id = item->at("snippet")["resourceId"]["videoId"];
        std::string& cat_id = vid_id_to_category_id[vid_id];
        if (cat_id != "10" && cat_id != "24") {
            std::cerr << "Ignoring videoId: " << vid_id
                      << ", not categorised as a song\n";
            continue;
        }

        Song song;
        std::string& title =
            item->at("snippet")["title"].get_ref<std::string&>();
        std::string& channel = item->at("snippet")["videoOwnerChannelTitle"]
                                   .get_ref<std::string&>();
        if (!parse_song_from_html(curl, vid_id, song) &&
            !parse_song_from_title(title, song)) {
            song.artists.push_back(std::move(channel));
            song.track = std::move(title);
        }

        Song sp_song = NewSpotifyAPI::get_ssot_song(curl, sp_access_tkn, song);
        if (sp_song.artists.empty()) {
            std::cerr
                << "Ignoring videoId: " << vid_id
                << ". Failed to determine artist and title information.\n";
            continue;
        }
        out_pl.items.data[sp_song].push_back(
            get_item(item->at("id").get_ref<std::string&>(), vid_id));
        cache.songs[vid_id] = std::move(sp_song);
    }
    cache.save();
    return true;
}

std::string search_song(CURL* curl, const std::string& access_tkn,
                        const Song& song) {
    std::ostringstream q;
    q << song.track;
    for (const std::string& a : song.artists) {
        q << ' ' << a;
    }

    std::string url = base_url + "/search";
    Params params = {{"part", "snippet"},
                     {"type", "video"},
                     {"videoCategoryId", "10"},
                     {"maxResults", "1"},
                     {"q", urlencode(q.str())}};
    json resp;
    long status_code = GET(curl, url, resp, params, access_tkn);
    if (status_code != 200L) {
        throw RequestError("Invalid response from google");
    }
    if (resp["items"].empty()) {
        return "";
    }
    return resp["items"][0]["id"]["videoId"];
}

// TODO make async
static std::string add_playlist_item(CURL* curl, const std::string& access_tkn,
                                     const std::string& pl_id,
                                     const std::string& vid_id) {
    std::string url = base_url + "/playlistItems";
    json resource_id = {{"kind", "youtube#video"}, {"videoId", vid_id}};
    json snippet = {{"resourceId", resource_id}, {"playlistId", pl_id}};
    json data = {{"snippet", snippet}};
    json resp;
    Params params = {{"part", "snippet"}};
    long status_code = POST(curl, url, data.dump(), resp, "application/json",
                            params, access_tkn);
    if (status_code != 200L) {
        throw RequestError("invalid response from google");
    }
    return resp["id"];
}

void playlist_items_add(CURL* curl, const std::string& access_tkn, Playlist& pl,
                        const SongCounts& song_cnts) {
    for (const auto& [song, cnt] : song_cnts) {
        std::string vid_id;
        if (pl.items.data.count(song)) {
            const std::string& item = pl.items.data[song].back();
            auto beg = std::find(item.begin(), item.end(), ':') + 1;
            vid_id = std::string(beg, item.end());
        } else {
            vid_id = NewYoutubeAPI::search_song(curl, access_tkn, song);
        }

        // TODO test
        for (int i = 0; i != cnt; i++) {
            std::string item_id =
                add_playlist_item(curl, access_tkn, pl.id, vid_id);
            pl.items.data[song].push_back(get_item(item_id, vid_id));
        }
    }
}

// TODO make async
static void remove_playlist_item(CURL* curl, const std::string& access_tkn,
                                 const std::string& id) {
    std::string url = base_url + "/playlistItems";
    Params params = {{"id", id}};
    json resp;
    long status_code = DELETE(curl, url, resp, access_tkn, params);
    if (status_code != 204L) {
        throw RequestError("Invalid response from google");
    }
}

void playlist_items_remove(CURL* curl, const std::string& access_tkn,
                           Playlist& pl, const SongCounts& song_cnts) {
    for (const auto& [song, cnt] : song_cnts) {
        for (int i = 0; i != cnt; i++) {
            const std::string& item = pl.items.data[song].back();
            auto end = std::find(item.begin(), item.end(), ':');
            std::string item_id(item.begin(), end);
            NewYoutubeAPI::remove_playlist_item(curl, access_tkn, item_id);
            pl.items.data[song].pop_back();
        }
    }
}

} // namespace NewYoutubeAPI

namespace NewSpotifyAPI {

static int BATCH_SIZE = 100;

static std::string read_etag_header(CURL* curl) {
    struct curl_header* header;
    CURLHcode status =
        curl_easy_header(curl, "etag", 0, CURLH_HEADER, -1, &header);
    if (status == CURLHE_MISSING) {
        return "";
    }
    if (status != CURLHE_OK) {
        throw RequestError("couldn't read etag header");
    }
    return std::string(header->value);
}

static long GET_paginated(CURL* curl, const std::string& url,
                          json& initial_page, Params& params,
                          const std::string& access_tkn, std::string& etag) {
    std::size_t limit = 50, offset = 0;
    params.emplace_back("limit", std::to_string(limit));
    params.emplace_back("offset", std::to_string(offset));

    long status_code = GET(curl, url, initial_page, params, access_tkn, etag);
    if (status_code < 200 || status_code >= 300) {
        return status_code;
    }
    etag = read_etag_header(curl);
    std::size_t total = initial_page["total"];

    for (offset = limit; offset < total; offset += limit) {
        params.back().second = std::to_string(offset);
        json next_page;
        status_code = GET(curl, url, next_page, params, access_tkn);
        if (status_code < 200 || status_code >= 300) {
            return status_code;
        }
        std::move(next_page["items"].begin(), next_page["items"].end(),
                  std::back_inserter(initial_page["items"]));
    }
    return status_code;
}

bool was_playlist_deleted(CURL* curl, const std::string& access_tkn,
                          const std::string& id) {
    std::string url = base_url + "/me/library/contains";
    json resp;
    Params params = {{"uris", urlencode("spotify:playlist:" + id)}};
    long status_code = GET(curl, url, resp, params, access_tkn);
    if (status_code != 200L) {
        throw RequestError("invalid response from spotify");
    }
    return !resp[0];
}

bool get_playlist(CURL* curl, const std::string& access_tkn,
                  const std::string& id, const std::string& etag,
                  Playlist& res) {
    std::ostringstream url(base_url, std::ios::ate);
    url << "/playlists/" << id;
    json resp;
    // TODO fields query param
    long response_code = GET(curl, url.str(), resp, {}, access_tkn, etag);

    if (response_code == 304L) {
        return false;
    } else if (response_code == 200L) {
        if (was_playlist_deleted(curl, access_tkn, id)) {
            res = Playlist();
            return true;
        }
        // get etag from response header
        struct curl_header* header;
        if (curl_easy_header(curl, "etag", 0, CURLH_HEADER, -1, &header) !=
            CURLHE_OK) {
            throw RequestError("couldn't read etag header");
        }

        const char* etag = header->value;
        const char* beg = std::find(etag, etag + std::strlen(etag), '"') + 1;
        const char* end = std::find(beg, etag + std::strlen(etag), '"');
        // base64 can have trailing eq signs
        end = std::find(beg, end, '=');
        res = Playlist(resp["id"], /*etag=*/std::string(beg, end),
                       resp["snapshot_id"], resp["name"], Platform::SPOTIFY,
                       !resp["public"], resp["items"]["total"]);
        return true;
    } else {
        throw RequestError("invalid response from spotify");
    }
}

Playlist create_playlist(CURL* curl, const std::string& access_tkn,
                         const std::string& title) {
    std::string url = base_url;
    url += "/me/playlists";
    json data;
    data["name"] = title;
    data["public"] = false;
    data["description"] = "Created by plsync";

    json resp;
    long status_code =
        POST(curl, url, data.dump(), resp, "application/json", {}, access_tkn);
    if (status_code != 201) {
        throw RequestError("invalid response from spotify");
    }
    return Playlist(resp["id"], "", resp["snapshot_id"], resp["name"],
                    Platform::SPOTIFY, !resp["public"], 0);
}

bool get_playlist_items(CURL* curl, const std::string& access_tkn,
                        Playlist& out_pl) {
    std::string url = base_url + "/playlists/" + out_pl.id + "/items";
    Params params = {{"fields", "total,items.item(artists.name,name,uri)"}};
    json resp;
    long status_code =
        GET_paginated(curl, url, resp, params, access_tkn, out_pl.items.etag);
    if (status_code == 304L) {
        return false;
    } else if (status_code != 200L) {
        throw RequestError("invalid response from spotify");
    }

    out_pl.items.data.clear();
    for (json& item : resp["items"]) {
        Song song;
        for (json& artist : item["item"]["artists"]) {
            song.artists.push_back(
                std::move(artist["name"].get_ref<std::string&>()));
        }
        song.track = std::move(item["item"]["name"].get_ref<std::string&>());
        out_pl.items.data[song].push_back(
            std::move(item["item"]["uri"].get_ref<std::string&>()));
    }
    return true;
}

static json search(CURL* curl, const std::string& access_tkn,
                   const Song& song) {
    std::ostringstream query;
    query << "track:" << song.track;
    for (const std::string& artist : song.artists) {
        query << " artist:" << artist;
    }

    std::string url = base_url + "/search";
    Params params = {
        {"type", "track"}, {"limit", "1"}, {"q", urlencode(query.str())}};
    json resp;
    long response_code = GET(curl, url, resp, params, access_tkn);
    if (response_code != 200L) {
        throw RequestError("invalid response from spotify");
    }

    if (!resp.contains("tracks") || resp["tracks"]["items"].empty()) {
        return {};
    }
    return resp["tracks"]["items"][0];
}

Song get_ssot_song(CURL* curl, const std::string& access_tkn,
                   const Song& song) {
    json resp = search(curl, access_tkn, song);
    if (resp.empty()) {
        return {};
    }

    Song song_res;
    song_res.track = std::move(resp["name"].get_ref<std::string&>());
    for (json& a : resp["artists"]) {
        song_res.artists.push_back(
            std::move(a["name"].get_ref<std::string&>()));
    }
    return song_res;
}

std::string search_song(CURL* curl, const std::string& access_tkn,
                        const Song& song) {
    json resp = search(curl, access_tkn, song);
    return resp.empty() ? "" : resp["uri"];
}

static void add_uris_to_playlist(CURL* curl, const std::string& access_tkn,
                                 const std::string& pl_id,
                                 const std::vector<std::string>& uris) {
    int n = uris.size();
    std::stringstream ss;
    ss << base_url << "/playlists/" << pl_id << "/items";
    std::string url = ss.str();

    for (int i = 0; i < n; i += BATCH_SIZE) {
        int j = std::min(n, i + BATCH_SIZE);
        std::vector<std::string> batch(uris.begin() + i, uris.begin() + j);
        json resp, body = {{"uris", batch}};
        long status_code =
            POST(curl, url, body.dump(), resp, "application/json",
                 /*params=*/{}, access_tkn);
        if (status_code != 201L) {
            throw RequestError("invalid response from spotify");
        }
    }
}

void playlist_items_add(CURL* curl, const std::string& access_tkn, Playlist& pl,
                        const SongCounts& song_cnts) {
    std::vector<std::string> uris;
    std::unordered_map<Song, std::vector<std::string>> items_patch;
    for (const auto& [song, cnt] : song_cnts) {
        std::string uri;
        if (pl.items.data.count(song)) {
            uri = pl.items.data[song].back();
        } else {
            uri = search_song(curl, access_tkn, song);
        }
        std::fill_n(std::back_inserter(uris), cnt, uri);
        std::fill_n(std::back_inserter(items_patch[song]), cnt, uri);
    }

    add_uris_to_playlist(curl, access_tkn, pl.id, uris);
    for (const auto& [song, items] : items_patch) {
        std::move(items.begin(), items.end(),
                  std::back_inserter(pl.items.data[song]));
    }
}

static void
remove_uris_from_playlist(CURL* curl, const std::string& access_tkn,
                          const std::string& pl_id,
                          const std::unordered_set<std::string>& uris) {
    std::stringstream ss;
    ss << base_url << "/playlists/" << pl_id << "/items";
    std::string url = ss.str();

    int n = uris.size();
    std::vector<json> batch;
    auto uri = uris.begin();
    for (int i = 1; i <= n; i++) {
        batch.push_back({{"uri", *uri}});

        if (i % BATCH_SIZE == 0 || i == n) {
            json resp, body = {{"items", batch}};
            long status_code =
                DELETE(curl, url, resp, access_tkn, /*params=*/{}, body.dump());
            if (status_code != 200L) {
                throw RequestError("invalid response from spotify");
            }
            batch.clear();
        }
        ++uri;
    }
}

void playlist_items_remove(CURL* curl, const std::string& access_tkn,
                           Playlist& pl, const SongCounts& song_cnts) {
    std::unordered_set<std::string> remove_uris;
    std::vector<std::string> add_uris;
    for (const auto& [s, cnt] : song_cnts) {
        std::vector<std::string>& items = pl.items.data[s];
        std::sort(items.begin(), items.end());
        auto it = items.end();
        for (int i = 0; i != cnt; i++) {
            --it;
            remove_uris.insert(*it);
        }
        int remaining = std::count(items.begin(), it, *it);
        std::fill_n(std::back_inserter(add_uris), remaining, *it);
    }

    remove_uris_from_playlist(curl, access_tkn, pl.id, remove_uris);
    add_uris_to_playlist(curl, access_tkn, pl.id, add_uris);

    for (const auto& [s, cnt] : song_cnts) {
        for (int i = 0; i != cnt; i++) {
            pl.items.data[s].pop_back();
        }
    }
}

} // namespace NewSpotifyAPI
