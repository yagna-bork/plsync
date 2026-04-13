#include "../include/api.h"
#include "../include/platform.h"
#include "../include/spotify_api.h"
#include "../include/util.h"
#include "../include/youtube_api.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include <zlib.h>

std::string BaseAPI::full_url(const std::string& endpoint,
                              const Params& params) {
    std::ostringstream full_url;
    full_url << url << '/' << endpoint;
    for (std::size_t i = 0; i != params.size(); i++) {
        char sep = (i == 0) ? '?' : '&';
        full_url << sep << params[i].first << '=' << params[i].second;
    }
    return full_url.str();
}

std::string BaseAPI::decompress_gzip(std::filesystem::path file) {
    std::unique_ptr<gzFile_s, gzDeleter> gzf(gzopen(file.c_str(), "rb"));
    if (!gzf) {
        throw RequestError("couldn't open gzip file");
    }

    std::string decompressed;
    size_t bufsz = 512;
    std::string buf(bufsz, 0);
    int nread;
    while ((nread = gzread(gzf.get(), buf.data(), bufsz)) > 0) {
        std::copy(buf.begin(), buf.begin() + nread,
                  std::back_inserter(decompressed));
    }
    if (nread == -1) {
        throw RequestError("couldn't decompress response");
    }
    return decompressed;
}

long BaseAPI::status_code() {
    long status_code;
    if (curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status_code) !=
        CURLE_OK) {
        throw RequestError("couldn't retrieve http status code");
    }
    return status_code;
}

bool BaseAPI::is_response_json() {
    char* content_type;
    if (curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_TYPE, &content_type) !=
        CURLE_OK) {
        throw RequestError("couldn't retrieve http content type");
    }
    const char* app_json = "application/json";
    return std::equal(app_json, app_json + std::strlen(app_json), content_type);
}

long BaseAPI::GET(const std::string& endpoint, nlohmann::json& jresp,
                  const Params& params, const std::string& token,
                  const std::string& etag) {
    curl_easy_reset(curl.get());
    curl_easy_setopt(curl.get(), CURLOPT_URL,
                     full_url(endpoint, params).c_str());

    curl_slist_raii headers;
    headers.append("If-None-Match: " + etag);
    headers.append("Accept-Encoding: gzip");
    headers.append("User-Agent: plsync (gzip)");
    if (!token.empty()) {
        headers.append("Authorization: Bearer " + token);
    }
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());

    /* store response in a .gz (gzip) file */
    std::filesystem::path tmpdir;
    if (!ensure_tmpdir(tmpdir)) {
        throw RequestError("couldn't access temp directory");
    }
    std::filesystem::path resp_path =
        tmpdir / ("resp." + urlencode64(rndstr(8)) + ".gz");
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_fwrite_cb);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);

    // wrapped file stream in scope so its guaranteed
    // to be flushed before decompression
    {
        std::ofstream respf(resp_path, std::ios::binary);
        if (!respf) {
            throw RequestError("couldn't create response file");
        }
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &respf);
        if (curl_easy_perform(curl.get()) != CURLE_OK) {
            throw RequestError("curl request failed");
        }
    }

    std::string resp = decompress_gzip(resp_path);
    if (is_response_json() && !resp.empty()) {
        jresp = nlohmann::json::parse(resp);
    }
    return status_code();
}

long BaseAPI::POST(const std::string& endpoint, const Fields& fields,
                   nlohmann::json& jresp) {
    curl_easy_reset(curl.get());
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

    std::string resp;
    curl_easy_setopt(curl.get(), CURLOPT_URL, full_url(endpoint).c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1);
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, fields_str.c_str());
    curl_easy_perform(curl.get());

    if (is_response_json()) {
        jresp = nlohmann::json::parse(resp);
    }
    return status_code();
}

std::unique_ptr<BaseDataAPI>
BaseDataAPI::get_api(Platform platform, std::shared_ptr<CURL> curl,
                     const std::string& access_tkn) {
    if (platform == Platform::YOUTUBE) {
        return std::make_unique<YoutubeAPI>(curl, access_tkn);
    } else {
        return std::make_unique<SpotifyAPI>(curl, access_tkn);
    }
}

std::string BaseAuthAPI::generate_code_verifier() {
    std::string res;
    res.reserve(128);
    std::random_device rd;
    std::mt19937_64 gen64(rd());

    unsigned long long rnd_num;
    unsigned char byte;
    // between 0-65 and represents a char allowed in verifier
    int char_opt;
    for (int i = 0; i != 128; i += 8) {
        rnd_num = gen64();

        for (int j = 0; j != 8; j++) {
            byte = static_cast<unsigned char>(rnd_num);
            char_opt = byte % 66;

            if (char_opt < 26) { // A-Z
                res.push_back('A' + char_opt);
            } else if (char_opt < 52) { // a-z
                res.push_back('a' + (char_opt - 26));
            } else if (char_opt < 62) { // 0-9
                res.push_back('0' + (char_opt - 52));
            } else if (char_opt == 62) {
                res.push_back('-');
            } else if (char_opt == 63) {
                res.push_back('.');
            } else if (char_opt == 64) {
                res.push_back('_');
            } else {
                res.push_back('~');
            }
            rnd_num >>= 8;
        }
    }
    return res;
}

std::string BaseAuthAPI::get_auth_url() {
    verifier = generate_code_verifier();
    std::string digest;
    if (!sha256(verifier, digest)) {
        throw std::runtime_error("Failed to hash verifier");
    }
    std::string challenge = urlencode64(digest);
    state = urlencode64(rndstr(128));

    std::string redirect_url = "http://127.0.0.1";
    std::string scope = join(scopes, "+");
    std::string client_id = get_setting("client_id", platform);
    std::ostringstream auth_url;
    auth_url << auth_svr_url << '?' << "client_id=" << client_id << '&'
             << "redirect_uri=" << redirect_url << ":" << redirect_port << '&'
             << "response_type=code&"
             << "scope=" << scope << '&' << "code_challenge=" << challenge
             << '&' << "code_challenge_method=S256&"
             << "state=" << state;
    return auth_url.str();
}

bool BaseAuthAPI::collect_auth_code() {
    if (state.empty()) {
        throw SequenceError("state not initialised");
    }

    int status, listenfd, sockfd, yes = 1;
    struct addrinfo hints, *svr_info, *p;
    struct sockaddr_storage client_addr;
    socklen_t addr_len;
    std::string res;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6, whatever auth server decides
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // figure out host ip for me

    status = getaddrinfo(NULL, std::to_string(redirect_port).c_str(), &hints,
                         &svr_info);
    if (status != 0) {
        std::cerr << '\n'
                  << "Couldn't get host network information: "
                  << gai_strerror(status) << '\n';
        return false;
    }

    for (p = svr_info; p != nullptr; p = p->ai_next) {
        listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listenfd == -1) {
            continue;
        }
        status =
            setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        if (status == -1) {
            continue;
        }
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == -1) {
            continue;
        }
        break;
    }

    if (p == nullptr) {
        std::cerr << '\n' << "Couldn't connect listener socket" << '\n';
        return false;
    }
    if (listen(listenfd, /*backlog=*/5) == -1) {
        std::cerr << '\n' << "Couldn't listen for connections" << '\n';
        return false;
    }

    sockfd = accept(listenfd, (struct sockaddr*)&client_addr, &addr_len);
    close(listenfd);
    if (sockfd == -1) {
        std::cerr << '\n' << "Server crashed" << '\n';
        return false;
    }

    // stuff those pesky params into a map
    std::size_t buff_sz = 1024;
    std::vector<char> buff(buff_sz);
    size_t bytes_recv = recv(sockfd, buff.data(), buff_sz, 0);
    std::unordered_map<std::string, std::string> params;
    std::string param, val;
    char* beg = std::find(buff.data(), buff.data() + bytes_recv, '?') + 1;
    char* end = std::find(beg, buff.data() + bytes_recv, ' ');
    char *amp, *eq;
    do {
        amp = std::find(beg, end, '&');
        eq = std::find(beg, amp, '=');
        param = std::string(beg, eq);
        val = std::string(eq + 1, amp);
        params[param] = val;
        beg = amp + 1;
    } while (amp != end);

    if (params.count("error")) {
        res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nError "
              "occured: " +
              params["error"] + ". Please return to terminal and try again";
        send(sockfd, res.c_str(), res.size(), 0);
        std::cerr << '\n' << params["error"] << '\n';
        return false;
    }

    if (!params.count("state") || params["state"] != state) {
        std::cerr << "Blocked cross-site request forgery attempt" << '\n';
        return false;
    }
    res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nSuccess! Return "
          "to terminal to continue";
    send(sockfd, res.c_str(), res.size(), 0);
    close(sockfd);
    auth_code = params["code"];
    return true;
}

std::unique_ptr<BaseAuthAPI> BaseAuthAPI::get_api(Platform platform,
                                                  std::shared_ptr<CURL> curl) {
    if (platform == Platform::YOUTUBE) {
        return std::make_unique<YoutubeAuthAPI>(curl);
    } else {
        return std::make_unique<SpotifyAuthAPI>(curl);
    }
}

void BaseAuthAPI::validate_scopes(const std::string& granted) {
    for (const auto& scope : scopes) {
        if (!contains(granted, scope)) {
            throw RequestError("user didn't grant necessary scopes");
        }
    }
}
