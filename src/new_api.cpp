#include "../include/new_api.h"
#include "../include/util.h"
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <ios>
#include <filesystem>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

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

bool is_response_json(CURL* curl) {
	char *content_type;
	if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type) != CURLE_OK) {
		throw RequestError("couldn't retrieve http content type");
	}
	const char *app_json = "application/json";
	return std::equal(app_json, app_json+std::strlen(app_json), content_type);
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
	nlohmann::json &jresp, 
	const Params &params,
	const std::string &access_tkn,
	const std::string &etag
) {
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, append_params(url, params).c_str());
	
	curl_slist_raii headers;
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
	if (is_response_json(curl) && !resp.empty()) {
		jresp = nlohmann::json::parse(resp);
	}
	return status_code(curl);
}

}
