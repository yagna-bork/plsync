#ifndef GUARD_API_H
#define GUARD_API_H
#include "cache.h"
#include "platform.h"
#include <ctime>
#include <curl/curl.h>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

/*
 * Abstract base class for platform-specific API clients.
 * Do not inherit from this directly. Inherit from
 * the other two ABCs instead.
 */
class BaseAPI {
public:
    class RequestError : public std::runtime_error {
    public:
        RequestError(const char* msg) : std::runtime_error(msg) {}
    };

    typedef std::vector<std::pair<std::string, std::string>> Fields;
    typedef std::vector<std::pair<std::string, std::string>> Params;

public:
    /*
     * TODO figure out how to test without making public
     * Performs a POST request at the specified endpoint.
     * This will use the default urlencoded POST type.
     * Throws RequestError if the request couldn't be made.
     * If response is JSON then it's saved in `resp`
     * and returns the http response status code.
     */
    long POST(const std::string& endpoint, const Fields& fields,
              nlohmann::json& resp);

    /*
     * TODO figure out how to test without making public
     * Performs a GET request at the specified endpoint.
     * Throws RequestError if the request couldn't be made.
     * If response is JSON then it's saved in `resp`
     * and returns the http response status code.
     * If you provide an etag status code can also be 304.
     * You can provide an access token to make an
     * authenticated request.
     */
    long GET(const std::string& endpoint, nlohmann::json& resp,
             const Params& params = {}, const std::string& access_tkn = "",
             const std::string& etag = "");

    virtual ~BaseAPI() = default;

protected:
    BaseAPI(Platform p, const std::string& url, std::shared_ptr<CURL> curl)
        : platform(p), url(url), curl(curl) {}

    Platform platform;

private:
    /* concat url, endpoint & query parameters with
     * {url}/{endpoint}?{key}={val}&... format */
    std::string full_url(const std::string& endpoint,
                         const Fields& fields = Fields());

    /* throws RequestError if indeterminable */
    long status_code();

    /* throws RequestError if indeterminable */
    bool is_response_json();

    /* throws RequestError on failure */
    std::string decompress_gzip(std::filesystem::path file_path);

protected:
    const std::string url;
    std::shared_ptr<CURL> curl;
};

/*
 * Abstract base class for platform specific data APIs.
 */
class BaseDataAPI : public BaseAPI {
public:
    /* returns whether users playlists have changed according to etag */
    virtual bool get_playlists(std::vector<Playlist>& playlists,
                               std::string& etag) = 0;

    static std::unique_ptr<BaseDataAPI> get_api(Platform platform,
                                                std::shared_ptr<CURL> curl,
                                                const std::string& access_tkn);

protected:
    BaseDataAPI(Platform p, const std::string& url, std::shared_ptr<CURL> curl,
                const std::string& access_tkn = "")
        : BaseAPI(p, url, curl), access_tkn(access_tkn) {}

protected:
    const std::string access_tkn;
};

/*
 * Abstract base class only for platform specific authentication
 * APIs. Use BaseDataAPI for APIs that implement endpoints that can
 * be accessed once authentication is complete. The distinction is
 * required because authentication endpoints have a different
 * base url to data endpoints.
 */
class BaseAuthAPI : public BaseAPI {
public:
    /*
     * Used when error occurs due to an auth flow step being
     * invovked without invoking the prerequisite steps first
     */
    class SequenceError : public std::logic_error {
    public:
        SequenceError(const char* msg) : std::logic_error(msg) {}
    };

    struct AccessTokenResponse {
        std::string access_tkn;
        std::time_t access_duration;
        std::string refresh_tkn;

        AccessTokenResponse() = default;

        AccessTokenResponse(nlohmann::json&& resp)
            : access_tkn(std::move(resp.at("access_token"))),
              access_duration(std::move(resp.at("expires_in"))),
              refresh_tkn(std::move(resp.value("refresh_token", ""))) {}
    };

    struct TokenResponse : public AccessTokenResponse {
        std::string refresh_tkn;

        TokenResponse() = default;

        TokenResponse(nlohmann::json&& resp)
            : AccessTokenResponse(std::move(resp)),
              refresh_tkn(std::move(resp["refresh_token"])) {}
    };

public:
    std::string get_auth_url();

    /* returns whether it succeeded */
    bool collect_auth_code();

    virtual TokenResponse exchange_auth_code() = 0;

    virtual AccessTokenResponse
    refresh_access_tkn(const std::string& refresh_tkn) = 0;

    static std::unique_ptr<BaseAuthAPI> get_api(Platform platform,
                                                std::shared_ptr<CURL> curl);

protected:
    BaseAuthAPI(Platform p, const std::string& url, std::shared_ptr<CURL> curl,
                const std::vector<std::string>& scopes,
                const std::string& auth_svr_url, int redirect_port)
        : BaseAPI(p, url, curl), scopes(scopes), auth_svr_url(auth_svr_url),
          redirect_port(redirect_port) {}

    /* throws RequestError if user didn't grant some scopes */
    void validate_scopes(const std::string& granted);

    std::string verifier;
    std::string auth_code;

private:
    std::vector<std::string> scopes;
    std::string state;
    std::string auth_svr_url;
    int redirect_port;

    std::string generate_code_verifier();
};
#endif
