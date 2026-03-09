#ifndef GUARD_NEW_API_H
#define GUARD_NEW_API_H
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace API {

class RequestError : public std::runtime_error {
public:
	RequestError(const char *msg) : std::runtime_error(msg) {}
};

typedef std::vector<std::pair<std::string, std::string>> Params;

/* 
 * Performs a GET request at the specified url.
 * Throws RequestError if the request couldn't be made.
 * If response is JSON then it's saved in `resp`
 * and returns the http response status code.
 * If you provide an etag status code can also be 304.
 * You can provide an access token to make an 
 * authenticated request.
 */
long GET(
	CURL* curl,
	const std::string &url, 
	nlohmann::json &resp, 
	const Params &params = {}, 
	const std::string &access_tkn = "", 
	const std::string &etag = ""
);

}
#endif
