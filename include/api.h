#ifndef GUARD_API_H
#define GUARD_API_H
#include "platform.h"
#include <string>
#include <memory>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

/* Abstract base class for platform-specific API clients */
class BaseAPI {
public:
	BaseAPI(
		Platform p, const std::string &url, std::shared_ptr<CURL> curl, 
		const std::string &access_tkn = ""
	) : platform(p), url(url), access_tkn(access_tkn), curl(curl) 
	{
	}

	/* 
	 * Performs a GET request at the specified endpoint.
     * Throws std::runtime_error if the request couldn't be made.
	 * If response is JSON then it's saved in `resp`
     * and returns the http response status code.
	 * If you provide an etag status code can also be 304.
	 */
	long GET(const std::string &endpoint, nlohmann::json &resp, const std::string &etag = "");

protected:
	Platform platform;

private:
	const std::string url;
	const std::string access_tkn;
	std::shared_ptr<CURL> curl;
};
#endif
