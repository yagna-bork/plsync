#include "../include/new_api.h"
#include "../include/new_spotify_api.h"
#include <ios>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

using namespace API;
using namespace nlohmann;

namespace NewSpotifyAPI {

bool get_playlist(CURL* curl, std::string& access_tkn, const std::string& id, const std::string& etag, Playlist& res) {
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
		if (curl_easy_header(curl, "etag", 0, CURLH_HEADER, -1, &header) != CURLHE_OK) {
			throw RequestError("couldn't read etag header");
		}
		const char* etag = header->value;
		const char* beg = std::find(etag, etag+std::strlen(etag), '"') + 1;
		const char* end = std::find(beg, etag+std::strlen(etag), '"');
		end = std::find(beg, end, '='); // base64 can have trailing eq signs
		res.etag = std::string(beg, end);
		return true;
	} else {
		throw RequestError("invalid response from spotify");
	}
}

Playlist create_playlist(CURL* curl, std::string& access_tkn, const std::string& title) {
	std::string url = base_url;
	url += "/me/playlists";
	json data;
	data["name"] = title;
	data["public"] = false;
	data["description"] = "Created by plsync";

	json resp;
	long status_code = POST(curl, url, data.dump(), resp, "application/json", {}, access_tkn);
	if (status_code != 201) {
		throw RequestError("invalid response from spotify");
	}
	return Playlist(resp["id"], "", resp["snapshot_id"], resp["name"], !resp["public"], 0);
}

}
