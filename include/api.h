#ifndef GUARD_API_H
#define GUARD_API_H
#include "platform.h"
#include <ctime>
#include <string>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

/* 
 * Abstract base class for platform-specific API clients.
 * Do not inherit from this directly. Inherit from 
 * the other two ABCs instead.
 */
class BaseAPI {
public:
	typedef std::runtime_error RequestError;

	/*
	 * TODO figure out how to test without making public
	 * Performs a POST request at the specified endpoint.
	 * This will use the default urlencoded POST type.
     * Throws RequestError if the request couldn't be made.
	 * If response is JSON then it's saved in `resp`
     * and returns the http response status code.
     */
	long POST(
		const std::string &endpoint, 
		const std::vector<std::pair<std::string, std::string>> &fields, 
		nlohmann::json &resp
	);

protected:
	BaseAPI(
		Platform p, const std::string &url, std::shared_ptr<CURL> curl
	) : platform(p), url(url), curl(curl) 
	{
	}

	/* concat url and endpoint with %s/%s format */
	std::string full_url(const std::string &endpoint);

	/* throws RequestError if indeterminable */
	long status_code();

	/* throws RequestError if indeterminable */
	bool is_response_json();

	Platform platform;
	std::shared_ptr<CURL> curl;

private:
	const std::string url;
};

/* 
 * Abstract base class for platform specific data APIs.
 */
class BaseDataAPI : public BaseAPI {
public:
	/* 
	 * TODO figure out how to test without making public
	 * Performs a GET request at the specified endpoint.
     * Throws RequestError if the request couldn't be made.
	 * If response is JSON then it's saved in `resp`
     * and returns the http response status code.
	 * If you provide an etag status code can also be 304.
	 */
	long GET(const std::string &endpoint, nlohmann::json &resp, const std::string &etag = "");

protected:
	BaseDataAPI(
		Platform p, const std::string &url, std::shared_ptr<CURL> curl, 
		const std::string &access_tkn = ""
	): BaseAPI(p, url, curl), access_tkn(access_tkn)
	{
	}

private:
	const std::string access_tkn;

	/* throws RequestError on failure */
	std::string decompress_gzip(std::filesystem::path file_path);
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
	struct TokenResponse {
		TokenResponse() {}
		TokenResponse(nlohmann::json &&resp) 
			: access_tkn(std::move(resp["access_token"])), 
			  access_duration(std::move(resp["expires_in"])),
			  refresh_tkn(std::move(resp["refresh_token"]))
		{
		}
		
		TokenResponse(const TokenResponse &other) = delete;
		TokenResponse(TokenResponse &&other) = delete;
	
		TokenResponse &operator=(const TokenResponse &rhs) = delete;
		TokenResponse &operator=(TokenResponse &&rhs) {
			access_tkn = std::move(rhs.access_tkn);
			access_duration = std::move(rhs.access_duration);
			refresh_tkn = std::move(rhs.refresh_tkn);
			return *this;
		}
	
		std::string access_tkn;
		std::time_t access_duration;
		std::string refresh_tkn;
	};

	virtual TokenResponse exchange_auth_code(
		const std::string &auth_code, const std::string &verifier
	) = 0;

	virtual ~BaseAuthAPI() = default;

protected:
	BaseAuthAPI(
		Platform p, const std::string &url, std::shared_ptr<CURL> curl,
		const std::vector<std::string> &scopes
	): BaseAPI(p, url, curl), scopes(scopes)
	{
	}
	
	/* throws RequestError if user didn't grant some scopes */
	void validate_scopes(const std::string &granted);

private:
	std::vector<std::string> scopes;
};
#endif
