#include "../include/api.h"
#include "../include/client_secret.h"
#include "../include/emoji_codepoint_ranges.h"
#include "../include/util.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <libcred.hpp>
#include <openssl/evp.h>
#include <random>
#include <stdexcept>
#include <string>
#include <utf8proc.h>

static const std::string KEYCHAIN_SERVICE = "plsync-token-service";

bool save_access_tkn(Platform platform, const std::string& tkn,
                     std::time_t duration) {
    std::string acc = platform_title_lower(platform) + "-access-token";
    std::string expiry = std::to_string(time(nullptr) + duration);
    std::string pwd = tkn + ":" + expiry;
    std::string err;
    return libcred::set_password(KEYCHAIN_SERVICE, acc, pwd, &err) ==
           libcred::SUCCESS;
}

bool save_refresh_tkn(Platform platform, const std::string& tkn) {
    std::string acc = platform_title_lower(platform) + "-refresh-token";
    std::string err;
    return libcred::set_password(KEYCHAIN_SERVICE, acc, tkn, &err) ==
           libcred::SUCCESS;
}

template <class T> static T stot(const std::string& s) {
    const char* type = typeid(T).name();
    if (strcmp(type, "i") == 0) {
        return std::stoi(s);
    } else if (strcmp(type, "l") == 0) {
        return std::stol(s);
    } else if (strcmp(type, "x") == 0) {
        return std::stoll(s);
    } else {
        throw std::domain_error("type must be int, long or long long");
    }
}

static bool get_access_tkn(Platform platform, std::string& tkn,
                           std::time_t& expiry) {
    std::string acc = platform_title_lower(platform) + "-access-token";
    std::string pass, err;
    if (libcred::get_password(KEYCHAIN_SERVICE, acc, &pass, &err) !=
        libcred::SUCCESS) {
        return false;
    }
    std::string::iterator sep = std::find(pass.begin(), pass.end(), ':');
    tkn = std::string(pass.begin(), sep);
    std::string expiry_str(sep + 1, pass.end());
    expiry = stot<std::time_t>(expiry_str);
    return true;
}

static bool get_access_tkn(Platform platform, std::string& tkn) {
    std::time_t _;
    return get_access_tkn(platform, tkn, _);
}

static bool get_access_tkn(Platform platform, std::time_t& expiry) {
    std::string _;
    return get_access_tkn(platform, _, expiry);
}

bool get_refresh_tkn(Platform platform, std::string& tkn) {
    std::string acc = platform_title_lower(platform) + "-refresh-token";
    std::string err;
    return libcred::get_password(KEYCHAIN_SERVICE, acc, &tkn, &err) ==
           libcred::SUCCESS;
}

static bool is_access_tkn_valid(Platform platform) {
    std::time_t expiry;
    if (!get_access_tkn(platform, expiry)) {
        // not in keychain
        return false;
    }
    return expiry > time(nullptr);
}

bool is_refresh_tkn_valid(Platform platform) {
    std::string _;
    return get_refresh_tkn(platform, _);
}

bool get_or_fetch_access_tkn(Platform platform, std::shared_ptr<CURL> curl,
                             std::string& tkn) {
    if (is_access_tkn_valid(platform)) {
        return get_access_tkn(platform, tkn);
    }

    std::string refresh_tkn;
    if (!get_refresh_tkn(platform, refresh_tkn)) {
        return false;
    }

    std::unique_ptr<BaseAuthAPI> api = BaseAuthAPI::get_api(platform, curl);
    BaseAuthAPI::AccessTokenResponse resp;
    try {
        resp = api->refresh_access_tkn(refresh_tkn);
    } catch (const BaseAuthAPI::RequestError& e) {
        return false;
    }

    save_access_tkn(platform, resp.access_tkn, resp.access_duration);
    tkn = std::move(resp.access_tkn);
    if (platform == Platform::SPOTIFY &&
        !save_refresh_tkn(platform, resp.refresh_tkn)) {
        return false;
    }
    return true;
}

std::string get_or_refresh_access_tkn(Platform platform,
                                      std::shared_ptr<CURL> curl) {
    std::string tkn;
    if (is_access_tkn_valid(platform)) {
        if (!get_access_tkn(platform, tkn)) {
            throw TokenStorageAccessError();
        }
        return tkn;
    }

    std::string refresh_tkn;
    if (!get_refresh_tkn(platform, refresh_tkn)) {
        throw TokenStorageAccessError();
    }
    std::unique_ptr<BaseAuthAPI> api = BaseAuthAPI::get_api(platform, curl);
    BaseAuthAPI::AccessTokenResponse resp =
        api->refresh_access_tkn(refresh_tkn);
    save_access_tkn(platform, resp.access_tkn, resp.access_duration);
    if (platform == Platform::SPOTIFY &&
        !save_refresh_tkn(platform, resp.refresh_tkn)) {
        throw TokenStorageAccessError();
    }
    return std::move(resp.access_tkn);
}

const std::unordered_map<std::string, std::string>& get_config() {
    static std::unordered_map<std::string, std::string> CONFIG;
    // TODO change for prod
    static std::string CONFIG_PATH = "plsync.cfg";
    if (!CONFIG.empty()) {
        return CONFIG;
    }

    std::ifstream config(CONFIG_PATH);
    if (!config) {
        throw std::runtime_error("Config file not found. Reinstall plsync");
    }

    std::string line, name, val;
    std::string::iterator sep;
    while (std::getline(config, line)) {
        sep = std::find(line.begin(), line.end(), '=');
        name = std::string(line.begin(), sep);
        val = std::string(sep + 1, line.end());
        CONFIG[name] = val;
    }
    // fucking google...
    CONFIG["yt_client_secret"] = CLIENT_NOT_SO_SECRET;
    return CONFIG;
}

std::string get_setting(std::string name, const std::string prefix) {
    auto CONFIG = get_config();
    name = prefix + name;
    assert(CONFIG.count(name) != 0);
    return CONFIG[name];
}

std::string get_setting(std::string name) {
    return get_setting(name, /*prefix=*/"");
}

std::string get_setting(std::string name, Platform platform) {
    return get_setting(name, platform_abbrev(platform) + "_");
}

// https://docs.openssl.org/master/man7/ossl-guide-libcrypto-introduction/#using-algorithms-in-applications
static std::string sha(const std::string& type, const std::string& s) {
    std::unique_ptr<EVP_MD_CTX, void (&)(EVP_MD_CTX*)> ctx(EVP_MD_CTX_new(),
                                                           EVP_MD_CTX_free);
    if (!ctx) {
        throw std::runtime_error("failed to generate sha digest");
    }
    std::unique_ptr<EVP_MD, void (&)(EVP_MD*)> algo(
        EVP_MD_fetch(nullptr, type.c_str(), nullptr), EVP_MD_free);
    if (!algo) {
        throw std::runtime_error("failed to generate sha digest");
    }
    if (!EVP_DigestInit_ex(ctx.get(), algo.get(), nullptr)) {
        throw std::runtime_error("failed to generate sha digest");
    }
    if (!EVP_DigestUpdate(ctx.get(), s.data(), s.size())) {
        throw std::runtime_error("failed to generate sha digest");
    }

    int digest_len = EVP_MD_get_size(algo.get());
    std::vector<unsigned char> digest(digest_len);
    if (!EVP_DigestFinal_ex(ctx.get(), digest.data(), nullptr)) {
        throw std::runtime_error("failed to generate sha digest");
    }
    return std::string(digest.begin(), digest.end());
}

std::string sha256(const std::string& s) { return sha("SHA256", s); }

std::string sha1(const std::string& s) { return sha("SHA1", s); }

char getb64char(int value) {
    if (value < 26) {
        return 'A' + value;
    } else if (value < 52) {
        return 'a' + (value - 26);
    } else if (value < 62) {
        return '0' + (value - 52);
    } else if (value == 62) {
        return '-';
    } else {
        return '_';
    }
}

std::string urlencode64(const std::string& s, bool pad) {
    std::string encoded;
    unsigned short buffer = 0; // 2 char buffer
    int unread_bits = 0;
    unsigned char b64mask = 0x3F;
    unsigned char b64bits = 6;
    unsigned char b64val, b64char;

    for (unsigned char c : s) {
        buffer = (buffer << 8) + c;
        unread_bits += 8;
        while (unread_bits >= b64bits) {
            b64val = (buffer >> (unread_bits - 6)) & b64mask;
            encoded.push_back(getb64char(b64val));
            unread_bits -= b64bits;
        }
    }
    if (unread_bits) {
        unsigned char mask = 0xFF >> (8 - unread_bits);
        b64val = (buffer & mask);
        // make it b64bits long by padding with 0s
        b64val <<= (6 - unread_bits);
        encoded.push_back(getb64char(b64val));
    }

    // padding
    if (!pad) {
        return encoded;
    }
    if (s.size() % 3 == 1) {
        encoded.push_back('=');
        encoded.push_back('=');
    } else if (s.size() % 3 == 2) {
        encoded.push_back('=');
    }
    return encoded;
}

static bool is_unreserved(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '-') ||
           (c == '.') || (c == '_') || (c == '~');
}

std::string urlencode(const std::string& s) {
    int n = s.size();
    std::ostringstream res;
    for (int i = 0; i != n; i++) {
        if (is_unreserved(s[i])) {
            res << s[i];
        } else {
            res << '%' << bin_to_hex(s[i], /*upper=*/true);
        }
    }
    return res.str();
}

std::vector<std::string> split(const std::string& s, const std::string& ss) {
    std::vector<std::string> res;
    std::string::const_iterator j, i = s.cbegin();
    while (j != s.end()) {
        j = find_range(i, s.cend(), ss.cbegin(), ss.cend());
        res.emplace_back(i, j);
        i = j + ss.size();
    }
    return res;
}

std::string join(const std::vector<std::string>& strs, const std::string& sep) {
    std::string joined;
    for (std::size_t i = 0; i != strs.size(); i++) {
        if (i != 0) {
            joined.append(sep);
        }
        joined.append(strs[i]);
    }
    return joined;
}

bool contains(const std::string& s, const std::string& ss) {
    return find_range(s.begin(), s.end(), ss.begin(), ss.end()) != s.end();
}

size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* data_p) {
    std::string* data = static_cast<std::string*>(data_p);
    copy(ptr, ptr + nmemb, back_inserter(*data));
    return nmemb;
}

size_t curl_fwrite_cb(char* st, size_t size, size_t nmemb, void* file_p) {
    std::ofstream* file = static_cast<std::ofstream*>(file_p);
    size_t nbyte = size * nmemb;
    return file->write(st, nbyte) ? nbyte : 0;
}

std::string rndstr(size_t size) {
    assert(size % 8 == 0);
    std::string res;
    res.reserve(size);
    std::random_device rd;
    std::mt19937_64 gen64(rd());
    unsigned long long rnd_num;
    unsigned char byte;

    for (int i = 0; i != (size / 8); i++) {
        rnd_num = gen64();
        for (int j = 0; j != 8; j++) {
            byte = static_cast<unsigned char>(rnd_num);
            res.push_back(byte);
            rnd_num >>= 8;
        }
    }
    return res;
}

bool ensure_tmpdir(std::filesystem::path& tmpdir) {
    tmpdir = std::filesystem::temp_directory_path() / "plsync";
    if (std::filesystem::exists(tmpdir)) {
        return true;
    }
    if (!std::filesystem::create_directory(tmpdir)) {
        tmpdir.clear();
        return false;
    }
    return true;
}

// nibble = half a byte hehe
static char hex_bit_lower(char nibble) {
    if (nibble <= 9) {
        return '0' + nibble;
    } else {
        return 'a' + nibble - 10;
    }
}

std::string bin_to_hex(const std::string& data) {
    std::string res;
    for (unsigned char byte : data) {
        // most significant nibble
        res.push_back(hex_bit_lower(byte >> 4));
        // least significant nibble
        res.push_back(hex_bit_lower(byte & 15));
    }
    return res;
}

static char hex_bit_upper(char nibble) {
    if (nibble <= 9) {
        return '0' + nibble;
    } else {
        return 'A' + nibble - 10;
    }
}

std::string bin_to_hex(unsigned char c, bool upper) {
    std::string res;
    unsigned char msn = c >> 4;
    unsigned char lsn = c & 15;
    res.push_back(upper ? hex_bit_upper(msn) : hex_bit_lower(msn));
    res.push_back(upper ? hex_bit_upper(lsn) : hex_bit_lower(lsn));
    return res;
}

// lowercase hex char to a binary char
unsigned char hex_to_bin(char c) {
    if ('0' <= c && c <= '9') {
        return c - '0';
    }
    return c - 'a' + 10;
}

// lowercase hex string to a raw binary string
std::string hex_to_bin(const std::string& hex_str) {
    std::size_t n = hex_str.size();
    std::string res;
    res.reserve(n / 2);
    unsigned char byte;
    for (std::size_t i = 0; i != n; i += 2) {
        byte = (hex_to_bin(hex_str[i]) << 4) + hex_to_bin(hex_str[i + 1]);
        res.push_back(byte);
    }
    return res;
}

bool _is_emoji(utf8proc_int32_t cp, int lo, int hi) {
    if (lo == hi) {
        return false;
    }
    int mid = std::midpoint(lo, hi);
    if (emoji_cp_ranges[mid].first <= cp && cp <= emoji_cp_ranges[mid].second) {
        return true;
    } else if (cp <= emoji_cp_ranges[mid].first) {
        hi = mid;
        return _is_emoji(cp, lo, mid);
    } else {
        lo = mid + 1;
        return _is_emoji(cp, mid + 1, hi);
    }
}

inline bool is_emoji(utf8proc_int32_t cp) {
    return _is_emoji(cp, 0, emoji_cp_ranges.size());
}

// see notes/unicode-research.txt for algorithm decision log
size_t utf8_len(const std::string& str) {
    static const utf8proc_int32_t sot = 0x02;
    static const utf8proc_int32_t zwj = 0x200d;
    static const utf8proc_int32_t skin_mod_beg = 0x1F3FB,
                                  skin_mod_end = 0x1F3FF;
    static const utf8proc_int32_t vs15 = 0xFE0E, vs16 = 0xFE0F;

    auto bytes = reinterpret_cast<const utf8proc_uint8_t*>(str.data());
    auto bytes_end = bytes + str.size();
    size_t len = 0;

    utf8proc_int32_t prev_cp = sot, curr_cp;
    utf8proc_int32_t state = 0;
    utf8proc_ssize_t nread;
    size_t grapheme_width = 0;
    bool skip = false;
    while ((nread = utf8proc_iterate(bytes, bytes_end - bytes, &curr_cp)) > 0) {
        if (utf8proc_grapheme_break_stateful(prev_cp, curr_cp, &state)) {
            len += grapheme_width;
            grapheme_width = 0;
            skip = false;
        }
        if (skip) {
            continue;
        }

        grapheme_width += utf8proc_charwidth(curr_cp);
        bool is_width_two =
            (skin_mod_beg <= curr_cp && curr_cp <= skin_mod_end) ||
            (curr_cp == vs16) || (prev_cp == zwj && is_emoji(curr_cp));
        if (is_width_two) {
            grapheme_width = 2;
            skip = true;
        } else if (curr_cp == vs15) {
            grapheme_width -= utf8proc_charwidth(prev_cp) - 1;
        }
        bytes += nread;
        prev_cp = curr_cp;
    }
    len += grapheme_width;
    return len;
}

std::vector<std::string> get_access_tokens(std::shared_ptr<CURL> curl) {
    std::vector<std::string> plat_to_access_tkn(Platform::INVALID);
    for (int i = Platform::YOUTUBE; i != Platform::INVALID; i++) {
        Platform plat = static_cast<Platform>(i);
#ifndef NDEBUG
        if (plat == Platform::TEST)
            continue;
#endif
        plat_to_access_tkn[plat] = get_or_refresh_access_tkn(plat, curl);
    }
    return plat_to_access_tkn;
}
