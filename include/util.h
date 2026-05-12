#ifndef GUARD_UTIL_H
#define GUARD_UTIL_H
#include "platform.h"
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

/*
 * A collection of functions used by this project
 * that could be useful for other projects too.
 */

/* read keychain for OAuth tokens */
class TokenStorageAccessError : public std::runtime_error {
public:
    TokenStorageAccessError() : std::runtime_error("") {}
};

/* throws TokenStorageAccessError on failure */
void save_access_tkn(Platform platform, const std::string& tkn,
                     std::time_t duration);
/* throws TokenStorageAccessError on failure */
void save_refresh_tkn(Platform platform, const std::string& tkn);

/*
 * Get access token or refresh it if it's expired.
 * Only call this if you're sure that refresh_token
 * is valid. Use is_refresh_tkn_valid to check
 * otherwise you must initialise the platform,
 * see 'init.h'.
 * throws TokenStorageAccessError or API::RequestError on failure.
 */
void get_or_fetch_access_tkn(Platform platform, std::shared_ptr<CURL> curl,
                             std::string& tkn);
/* throws TokenStorageAccessError on storage access or API::RequestError on
 * token refresh failure */
std::string get_or_refresh_access_tkn(Platform platform,
                                      std::shared_ptr<CURL> curl);
/* throws TokenStorageAccessError on failure */
void get_refresh_tkn(Platform platform, std::string& tkn);

bool is_refresh_tkn_valid(Platform platform);

/* read config file */
std::string get_setting(std::string name);
std::string get_setting(std::string name, Platform platform);

/* Throws std::runtime_error upon failure */
std::string sha256(const std::string& s);
std::string sha1(const std::string& s);

// https://en.wikipedia.org/wiki/Base64
std::string urlencode64(const std::string& s, bool pad = false);

// https://en.wikipedia.org/wiki/Percent-encoding
std::string urlencode(const std::string& s);

template <class RndIt>
RndIt find_range(RndIt first1, RndIt last1, RndIt first2, RndIt last2) {
    if (first1 == last1) {
        return last1;
    }
    if (first2 == last2) {
        return first1;
    }
    int size1 = last1 - first1;
    int size2 = last2 - first2;
    if (size2 > size1) {
        return last1;
    }

    // pre-processing
    std::vector<int> fallback(size2);
    fallback[0] = -1;
    for (int pos = 1, cnd = 0; pos != size2; pos++, cnd++) {
        if (first2[pos] == first2[cnd]) {
            fallback[pos] = fallback[cnd];
        } else {
            fallback[pos] = cnd;
            while (cnd >= 0 && first2[pos] != first2[cnd]) {
                cnd = fallback[cnd];
            }
        }
    }

    // post-pre-processing :)
    for (int pos = 0, cnd = 0; pos != size1; pos++, cnd++) {
        if (first1[pos] == first2[cnd]) {
            if (cnd == size2 - 1) {
                return first1 + pos - cnd;
            }
        } else {
            while (cnd >= 0 && first1[pos] != first2[cnd]) {
                cnd = fallback[cnd];
            }
        }
    }
    return last1;
}

std::vector<std::string> split(const std::string& st, const std::string& subst);

std::string join(const std::vector<std::string>& strs, const std::string& sep);

bool contains(const std::string& s, const std::string& ss);

/* Pass to curl WRITE_FUNCTION opt */
size_t curl_write_cb(char*, size_t, size_t, void*);

size_t curl_fwrite_cb(char*, size_t, size_t, void*);

/*
 * RAII alternative to curl string lists which can
 * be used to add headers to a request
 */
class curl_slist_raii {
public:
    curl_slist_raii() : p(nullptr) {}

    // no copy or assignment
    curl_slist_raii(const curl_slist_raii&) = delete;
    curl_slist_raii& operator=(const curl_slist_raii&) = delete;

    void append(const char* s) { p = curl_slist_append(p, s); }
    void append(const std::string& s) { append(s.c_str()); }

    struct curl_slist* get() { return p; }

    ~curl_slist_raii() { curl_slist_free_all(p); }

private:
    struct curl_slist* p;
};

struct gzDeleter {
    void operator()(gzFile f) { gzclose(f); }
};

struct fileDeleter {
    void operator()(std::FILE* f) { std::fclose(f); }
};

/* size must be a multiple of 8 */
std::string rndstr(size_t size);

bool ensure_tmpdir(std::filesystem::path& tmpdir);

template <class FstreamT, class... Types>
FstreamT ensure_bin_file(const std::filesystem::path& file,
                         std::ios::openmode mode = std::ios::binary) {
    std::filesystem::create_directories(file.parent_path());
    if (!std::filesystem::exists(file)) {
        std::ofstream tmp(file);
    }
    return FstreamT(file, mode | std::ios::binary);
}

inline std::shared_ptr<CURL> get_curl() {
    return std::shared_ptr<CURL>(curl_easy_init(), curl_easy_cleanup);
}

std::string bin_to_hex(const std::string& data);
std::string bin_to_hex(unsigned char c, bool upper);
std::string hex_to_bin(const std::string& hex_str);

size_t utf8_len(const std::string& s);

/* May throw TokenStorageAccessError */
std::vector<std::string> get_access_tokens(std::shared_ptr<CURL> curl);
#endif
