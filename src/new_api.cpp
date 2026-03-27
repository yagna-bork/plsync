#include "../include/new_api.h"
#include "../include/new_spotify_api.h"
#include "../include/new_youtube_api.h"
#include "../include/platform.h"
#include "../include/models.h"
#include "../include/util.h"
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <ios>
#include <stdexcept>
#include <filesystem>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace nlohmann;

// TODO reduce the number of files by bringing spotify
// and youtube implementation files into this single file
namespace API {

std::string append_params(const std::string &url, const Params &params) {
	std::ostringstream ss(url, std::ios::ate);
	for (std::size_t i = 0; i != params.size(); i++) {
		char sep = (i == 0) ? '?' : '&';
		ss << sep << params[i].first << '=' << params[i].second;
	}
	return ss.str();
}

std::string decompress_gzip(std::filesystem::path file) {
	gzFile_s* gzf = gzopen(file.c_str(), "rb");
	if (!gzf) {
		gzclose(gzf);
		throw RequestError("couldn't open gzip file");
	}

	std::string decompressed;
	size_t bufsz = 512;
	std::string buf(bufsz, 0);
	int nread;
	while ((nread = gzread(gzf, buf.data(), bufsz)) > 0) {
		std::copy(buf.begin(), buf.begin()+nread, std::back_inserter(decompressed));
	}
	gzclose(gzf);
	if (nread == -1) {
		throw RequestError("couldn't decompress response");
	}
	return decompressed;
}

long status_code(CURL* curl) {
	long status_code;
	if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code) != CURLE_OK) {
		throw RequestError("couldn't retrieve http status code");
	}
	return status_code;
}

long GET(
	CURL* curl,
	const std::string &url, 
	json &jresp, 
	const Params &params,
	const std::string &access_tkn,
	const std::string &etag
) {
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, append_params(url, params).c_str());
	
	curl_slist_raii headers;
	headers.append("Accept: application/json");
	headers.append("If-None-Match: " + etag);
	headers.append("Accept-Encoding: gzip");
	headers.append("User-Agent: plsync (gzip)");
	if (!access_tkn.empty()) {
		headers.append("Authorization: Bearer " + access_tkn);
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());

	/* store response in a .gz (gzip) file */
	std::filesystem::path tmpdir;
	if (!ensure_tmpdir(tmpdir)) {
		throw RequestError("couldn't access temp directory");
	}
	std::filesystem::path resp_path = tmpdir / ("resp." + urlencode64(rndstr(8)) + ".gz");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite_cb);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

	// wrapped file stream in scope so its guaranteed 
	// to be flushed before decompression
	{
		std::ofstream respf(resp_path, std::ios::binary);
		if (!respf) {
			throw RequestError("couldn't create response file");
		}
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respf);
		if (curl_easy_perform(curl) != CURLE_OK) {
			throw RequestError("curl request failed");
		}
	}

	std::string resp = decompress_gzip(resp_path);
	jresp = json::parse(resp.empty() ? "{}" : resp);
	return status_code(curl);
}

long POST(
	CURL* curl, 
	const std::string &url, 
	const std::string& data, 
	json &jresp, 
	const std::string& application_type,
	const Params &params,
	const std::string& access_tkn
) {
	curl_easy_reset(curl);

	std::string resp;
	curl_easy_setopt(curl, CURLOPT_URL, append_params(url, params).c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

	curl_slist_raii headers;
	headers.append("Accept: application/json");
	if (!application_type.empty()) {
		headers.append("Content-Type: " + application_type);
	}
	if (!access_tkn.empty()) {
		headers.append("Authorization: Bearer " + access_tkn);
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());

	curl_easy_perform(curl);
	jresp = json::parse(resp.empty() ? "{}" : resp);
	return status_code(curl);
}

std::string fields_to_string(const Fields& fields) {
	std::string fields_str;
	for (int i = 0; i != fields.size(); i++) {
		const auto &field = fields[i];
		if (i > 0) {
			fields_str.push_back('&');
		}
		fields_str.append(field.first);
		fields_str.push_back('=');
		fields_str.append(field.second);
	}
	return fields_str;
}

bool get_playlist(
	Platform plat, CURL* curl, std::string& access_tkn, const std::string& id, const std::string& etag, Playlist& res
) {
	switch (plat) {
		case Platform::YOUTUBE:
			return NewYoutubeAPI::get_playlist(curl, access_tkn, id, etag, res);
			break;
		case Platform::SPOTIFY:
			return NewSpotifyAPI::get_playlist(curl, access_tkn, id, etag, res);
			break;
		default:
			throw std::domain_error("function not yet implemented for " + platform_title_lower(plat));
	}
}

Playlist create_playlist(Platform plat, CURL* curl, std::string& access_tkn, const std::string& title) {
	switch (plat) {
		case Platform::YOUTUBE:
			return NewYoutubeAPI::create_playlist(curl, access_tkn, title);
			break;
		case Platform::SPOTIFY:
			return NewSpotifyAPI::create_playlist(curl, access_tkn, title);
			break;
		default:
			throw std::domain_error("function not yet implemented for " + platform_title_lower(plat));
	}
}

}
