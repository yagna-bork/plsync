#include "../include/api.h"
#include "../include/util.h"
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <curl/curl.h>
#include <zlib.h>
#include <nlohmann/json.hpp>

std::filesystem::path ensure_tmpdir() {
	std::filesystem::path tmpdir = std::filesystem::temp_directory_path() / "plsync";
	if (std::filesystem::exists(tmpdir)) {
		return tmpdir;
	}
	if (!std::filesystem::create_directory(tmpdir)) {
		throw std::runtime_error("couldn't create tmp directory");
	}
	return tmpdir;
}

long BaseAPI::GET(const std::string &endpoint, nlohmann::json &jresp, const std::string &etag) {
	curl_easy_reset(curl.get());
	std::string full_url(url.size() + endpoint.size() + 2, 0);
	snprintf(full_url.data(), full_url.size(), "%s/%s", url.c_str(), endpoint.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_URL, full_url.c_str());
	
	curl_slist_raii headers;
	headers.append("If-None-Match: " + etag);
	headers.append("Accept-Encoding: gzip");
	headers.append("User-Agent: plsync (gzip)");
	if (!access_tkn.empty()) {
		headers.append("Authorization: Bearer " + access_tkn);
	}
	curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());

	/* store response in a .gz (gzip) file */
	std::filesystem::path tmpdir = ensure_tmpdir();
	std::filesystem::path resp_path = tmpdir / ("resp." + urlencode64(rndstr(8)) + ".gz");
	curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_fwrite_cb);
	curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);

	// wrap in scope so file stream is automatically flushed
	// and zlib can take over handling it
	{
		std::ofstream respf(resp_path, std::ios::binary);
		if (!respf) {
			throw std::runtime_error("couldn't create response file");
		}
		curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &respf);
		if (curl_easy_perform(curl.get()) != CURLE_OK) {
			throw std::runtime_error("curl request failed");
		}
	}

	/* decompress the response */
	std::unique_ptr<gzFile_s, gzDeleter> gzf(gzopen(resp_path.c_str(), "rb"));
	if (!gzf) {
		throw std::runtime_error("couldn't open response file");
	}

	std::string resp;
	size_t bufsz = 512;
	std::string buf(bufsz, 0);
	int nread;
	while ((nread = gzread(gzf.get(), buf.data(), bufsz)) > 0) {
		std::copy(buf.begin(), buf.begin()+nread, std::back_inserter(resp));
	}
	if (nread == -1) {
		throw std::runtime_error("couldn't decompress response");
	}

	long status_code;
	if (curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status_code) != CURLE_OK) {
		throw std::runtime_error("couldn't retrieve http status code");
	}

	char *content_type;
	if (curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_TYPE, &content_type) != CURLE_OK) {
		throw std::runtime_error("couldn't retrieve http status code");
	}
	const char *app_json = "application/json";
	if (std::equal(app_json, app_json+std::strlen(app_json), content_type)) {
		jresp = nlohmann::json::parse(resp);
	}
	return status_code;
}
