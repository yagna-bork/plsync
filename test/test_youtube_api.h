#ifndef GUARD_TEST_YOUTUBE_API_H
#define GUARD_TEST_YOUTUBE_API_H
#include "../include/youtube_api.h"
#include <cstdlib>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <curl/curl.h>

namespace TestYoutubeAPI {

void test_get_video() {
	const char *key = std::getenv("YT_API_KEY");
	assert(key != nullptr);
	std::string endpoint = std::string("videos?id=7lCDEYXw3mM&fields=items(id,snippet(channelId,title))"
									   "&part=snippet&key=") + key;
	nlohmann::json res;
	long status_code;
	std::shared_ptr<CURL> curl(curl_easy_init(), curl_easy_cleanup);
	YoutubeAPI ytapi(curl);

	try {
		status_code = ytapi.GET(endpoint, res);
	} catch (const std::runtime_error &e) {
		std::cout << "test_get_video(): FAILED\n";
		return;
	}
	if (status_code != 200) {
		std::cout << "test_get_video(): FAILED\n";
		return;
	}
	
	nlohmann::json expected = nlohmann::json::parse(R"({
	  "items": [{
		  "id": "7lCDEYXw3mM",
		  "snippet": {
			"channelId": "UC_x5XG1OV2P6uZZ5FSM9Ttw",
			"title": "Google I/O 101: Q&A On Using Google APIs"
		  }
		}]
	})");

	if (res == expected) {
		std::cout << "test_get_video(): PASSED\n";
	} else {
		std::cout << "test_get_video(): FAILED\n";
	}
}

void run() {
	test_get_video();
}

}
#endif
