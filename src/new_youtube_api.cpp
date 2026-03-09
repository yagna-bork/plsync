#include "../include/new_youtube_api.h"
#include "../include/new_api.h"

namespace NewYoutubeAPI {

bool get_playlist(
	CURL* curl, std::string& access_tkn, const std::string& id, const std::string& etag, Playlist& res
) {
	std::string url = base_url + "/playlists";
	API::Params params = {
		{"id", id},
		{"part", "id,snippet,status,contentDetails"},
		{
			"fields", 
			"etag,items("
				"id,snippet/title,status/privacyStatus,contentDetails/itemCount"
			")"
		},
	};
	nlohmann::json resp;
	long status_code = API::GET(curl, url, resp, params, access_tkn, etag);

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
		res.title = resp["items"][0]["snippet"]["title"];
		res.is_private = (resp["items"][0]["status"]["privacyStatus"] == "private");
		res.items = resp["items"][0]["contentDetails"]["itemCount"];
		return true;
	} else {
		throw API::RequestError("Invalid response from youtube");
	}
}

}
