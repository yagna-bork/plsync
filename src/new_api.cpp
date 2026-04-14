#include "../include/cache.h"
#include "../include/new_api.h"
#include "../include/platform.h"
#include "../include/util.h"
#include <algorithm>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

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
    std::string hash;
    sha256(str.str(), hash);
    return hash;
}

static void song_counts_insert(SongCounts& song_counts, const Song& song) {
    if (!song_counts.count(song)) {
        song_counts[song] = 0;
    }
    ++song_counts[song];
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

bool get_playlist(Platform plat, CURL* curl, const std::string& access_tkn,
                  const std::string& id, const std::string& etag,
                  Playlist& res) {
    switch (plat) {
    case Platform::YOUTUBE:
        return NewYoutubeAPI::get_playlist(curl, access_tkn, id, etag, res);
        break;
    case Platform::SPOTIFY:
        return NewSpotifyAPI::get_playlist(curl, access_tkn, id, etag, res);
        break;
    default:
        throw std::domain_error("function not yet implemented for " +
                                platform_title_lower(plat));
    }
}

Playlist create_playlist(Platform plat, CURL* curl,
                         const std::string& access_tkn,
                         const std::string& title) {
    switch (plat) {
    case Platform::YOUTUBE:
        return NewYoutubeAPI::create_playlist(curl, access_tkn, title);
        break;
    case Platform::SPOTIFY:
        return NewSpotifyAPI::create_playlist(curl, access_tkn, title);
        break;
    default:
        throw std::domain_error("function not yet implemented for " +
                                platform_title_lower(plat));
    }
}

bool get_song_counts(Platform plat, CURL* curl,
                     const std::string& plat_access_tkn,
                     const std::string& sp_access_tkn,
                     const std::string& playlist_id,
                     SongCounts& out_song_counts, std::string& in_out_etag) {
    switch (plat) {
    case Platform::YOUTUBE:
        return NewYoutubeAPI::get_song_counts(curl, plat_access_tkn,
                                              sp_access_tkn, playlist_id,
                                              out_song_counts, in_out_etag);
        break;
    case Platform::SPOTIFY:
        return NewSpotifyAPI::get_song_counts(
            curl, plat_access_tkn, playlist_id, out_song_counts, in_out_etag);
        break;
    default:
        throw std::domain_error("function not yet implemented for " +
                                platform_title_lower(plat));
    }
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
            return true;
        }
        res.id = resp["items"][0]["id"];
        // the api has a seperate etag for the resource containing
        // a single playlist and the playlist resource itself
        res.etag = resp["etag"];
        res.version = resp["items"][0]["etag"];
        res.title = resp["items"][0]["snippet"]["title"];
        res.is_private =
            (resp["items"][0]["status"]["privacyStatus"] == "private");
        res.items = resp["items"][0]["contentDetails"]["itemCount"];
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
                    std::move(resp["snippet"]["title"]),
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
    if (!m[6].matched) {
        std::vector<std::string> features =
            split(std::string(m[6].first, m[6].second), ", ");
        std::move(features.begin(), features.end(),
                  std::back_inserter(out_song.artists));
    }
    return true;
}

bool get_song_counts(CURL* curl, const std::string& yt_access_tkn,
                     const std::string& sp_access_tkn,
                     const std::string& playlist_id,
                     SongCounts& out_song_counts, std::string& in_out_etag) {
    std::string url = base_url + "/playlistItems";
    Params params = {{"part", "snippet,contentDetails"},
                     {"playlistId", playlist_id},
                     {"maxResults", "50"}};
    json pl_items_resp;
    long status_code = GET_paginated(curl, url, pl_items_resp, params,
                                     yt_access_tkn, in_out_etag);

    if (status_code == 304L) {
        return false;
    } else if (status_code != 200L) {
        throw RequestError("Invalid response from google");
    }
    in_out_etag = pl_items_resp["etag"];
    json& pl_items = pl_items_resp["items"];
    SongCache song_cache = load_song_cache(Platform::YOUTUBE);

    std::vector<json*> new_pl_items;
    std::unordered_set<std::string> vid_ids_set;
    for (json& item : pl_items) {
        const std::string& id =
            item["snippet"]["resourceId"]["videoId"].get_ref<std::string&>();
        if (song_cache.count(id)) {
            song_counts_insert(out_song_counts, song_cache[id]);
        } else {
            new_pl_items.push_back(&item);
            vid_ids_set.insert(id);
        }
    }

    std::unordered_map<std::string, std::string> video_id_to_category_id;
    url = "https://www.googleapis.com/youtube/v3/videos";
    params = {{"part", "id,snippet"}, {"maxResults", "50"}, {"id", ""}};

    int n = vid_ids_set.size();
    auto vid_id_it = vid_ids_set.cbegin();
    for (int i = 0; i < n; i += 50) {
        std::ostringstream vid_ids;
        for (int j = 0; (i + j != n) && (j != 50); j++) {
            if (j > 0) {
                vid_ids << ',';
            }
            vid_ids << *vid_id_it++;
        }
        params.back().second = vid_ids.str();
        json videos_resp;
        status_code = GET(curl, url, videos_resp, params, yt_access_tkn);
        const json& videos = videos_resp["items"];

        for (const json& vid : videos) {
            const std::string& id = vid["id"];
            const std::string& category_id = vid["snippet"]["categoryId"];
            video_id_to_category_id[id] = category_id;
        }
    }

    for (json* item : new_pl_items) {
        std::string id = item->at("snippet")["resourceId"]["videoId"];
        if (song_cache.count(id)) {
            song_counts_insert(out_song_counts, song_cache[id]);
            continue;
        }
        if (video_id_to_category_id[id] != "10") {
            std::cerr << "Ignoring videoId: " << id
                      << ". Not categorised as a song" << '\n';
            continue;
        }
        Song song;
        std::string& title =
            item->at("snippet")["title"].get_ref<std::string&>();
        std::string& channel = item->at("snippet")["videoOwnerChannelTitle"]
                                   .get_ref<std::string&>();
        if (!parse_song_from_html(curl, id, song) &&
            !parse_song_from_title(title, song)) {
            song.artists.push_back(channel);
            song.track = std::move(title);
        }

        Song sp_song = NewSpotifyAPI::search_song(curl, sp_access_tkn, song);
        if (sp_song.artists.empty()) {
            std::cerr
                << "Ignoring videoId: " << id
                << ". Failed to determine artist and title information.\n";
            continue;
        }
        song_counts_insert(out_song_counts, sp_song);
        song_cache[id] = std::move(sp_song);
    }
    save_song_cache(song_cache, Platform::YOUTUBE);
    return true;
}

} // namespace NewYoutubeAPI

namespace NewSpotifyAPI {

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
    } else if (response_code == 404L) {
        res = Playlist();
        return true;
    } else if (response_code == 200L) {
        res.id = resp["id"];
        res.title = resp["name"];
        res.is_private = !resp["public"];
        res.items = resp["items"]["total"];
        res.version = resp["snapshot_id"];

        // get etag from response header
        struct curl_header* header;
        if (curl_easy_header(curl, "etag", 0, CURLH_HEADER, -1, &header) !=
            CURLHE_OK) {
            throw RequestError("couldn't read etag header");
        }
        const char* etag = header->value;
        const char* beg = std::find(etag, etag + std::strlen(etag), '"') + 1;
        const char* end = std::find(beg, etag + std::strlen(etag), '"');
        end = std::find(beg, end, '='); // base64 can have trailing eq signs
        res.etag = std::string(beg, end);
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
                    !resp["public"], 0);
}

bool get_song_counts(CURL* curl, const std::string& access_tkn,
                     const std::string& playlist_id,
                     SongCounts& out_song_counts, std::string& in_out_etag) {
    std::string url = base_url + "/playlists/" + playlist_id + "/items";
    Params params = {{"fields", "total,items.item(artists.name,name)"}};
    json resp;
    long status_code =
        GET_paginated(curl, url, resp, params, access_tkn, in_out_etag);
    if (status_code == 304L) {
        return false;
    } else if (status_code != 200L) {
        throw RequestError("invalid response from spotify");
    }

    for (json& item : resp["items"]) {
        Song song;
        for (json& artist : item["item"]["artists"]) {
            song.artists.push_back(
                std::move(artist["name"].get_ref<std::string&>()));
        }
        song.track = std::move(item["item"]["name"].get_ref<std::string&>());
        song_counts_insert(out_song_counts, song);
    }
    return true;
}

Song search_song(CURL* curl, const std::string& access_tkn, const Song& song) {
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
    if (!resp.contains("tracks") || resp["tracks"].empty()) {
        return {};
    }

    json& search_res = resp["tracks"]["items"][0];
    Song song_res;
    song_res.track = std::move(search_res["name"]);
    for (json& artist : search_res["artists"]) {
        song_res.artists.push_back(std::move(artist["name"]));
    }
    return song_res;
}

} // namespace NewSpotifyAPI
