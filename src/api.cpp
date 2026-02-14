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

std::string BaseAPI::full_url(const std::string &endpoint) {
	std::string full_url = url;
	full_url.push_back('/');
	full_url.append(endpoint);
	return full_url;
}

std::string BaseDataAPI::decompress_gzip(std::filesystem::path file) {
	std::unique_ptr<gzFile_s, gzDeleter> gzf(gzopen(file.c_str(), "rb"));
	if (!gzf) {
		throw RequestError("couldn't open gzip file");
	}

	std::string decompressed;
	size_t bufsz = 512;
	std::string buf(bufsz, 0);
	int nread;
	while ((nread = gzread(gzf.get(), buf.data(), bufsz)) > 0) {
		std::copy(buf.begin(), buf.begin()+nread, std::back_inserter(decompressed));
	}
	if (nread == -1) {
		throw RequestError("couldn't decompress response");
	}
	return decompressed;
}

long BaseAPI::status_code() {
	long status_code;
	if (curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status_code) != CURLE_OK) {
		throw RequestError("couldn't retrieve http status code");
	}
	return status_code;
}

bool BaseAPI::is_response_json() {
	char *content_type;
	if (curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_TYPE, &content_type) != CURLE_OK) {
		throw RequestError("couldn't retrieve http response type");
	}
	const char *app_json = "application/json";
	return std::equal(app_json, app_json+std::strlen(app_json), content_type);
}

long BaseDataAPI::GET(const std::string &endpoint, nlohmann::json &jresp, const std::string &etag) {
	curl_easy_reset(curl.get());
	curl_easy_setopt(curl.get(), CURLOPT_URL, full_url(endpoint).c_str());
	
	curl_slist_raii headers;
	headers.append("If-None-Match: " + etag);
	headers.append("Accept-Encoding: gzip");
	headers.append("User-Agent: plsync (gzip)");
	if (!access_tkn.empty()) {
		headers.append("Authorization: Bearer " + access_tkn);
	}
	curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());

	/* store response in a .gz (gzip) file */
	std::filesystem::path tmpdir;
	if (!ensure_tmpdir(tmpdir)) {
		throw RequestError("couldn't access temp directory");
	}
	std::filesystem::path resp_path = tmpdir / ("resp." + urlencode64(rndstr(8)) + ".gz");
	curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_fwrite_cb);
	curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);

	// wrapped file stream in scope so its guaranteed 
	// to be flushed before decompression
	{
		std::ofstream respf(resp_path, std::ios::binary);
		if (!respf) {
			throw RequestError("couldn't create response file");
		}
		curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &respf);
		if (curl_easy_perform(curl.get()) != CURLE_OK) {
			throw RequestError("curl request failed");
		}
	}

	std::string resp = decompress_gzip(resp_path);
	if (is_response_json()) {
		jresp = nlohmann::json::parse(resp);
	}
	return status_code();
}

long BaseAPI::POST(
	const std::string &endpoint, 
	const std::vector<std::pair<std::string, std::string>> &fields, 
	nlohmann::json &jresp
) {
	curl_easy_reset(curl.get());
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

	std::string resp;
	curl_easy_setopt(curl.get(), CURLOPT_URL, full_url(endpoint).c_str());
	curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &resp);
	curl_easy_setopt(curl.get(), CURLOPT_POST, 1);
	curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, fields_str.c_str());
	curl_easy_perform(curl.get());
	
	if (is_response_json()) {
		jresp = nlohmann::json::parse(resp);
	}
	return status_code();
}

void BaseAuthAPI::validate_scopes(const std::string &granted) {
	for (const auto &scope: scopes) {
		if (!contains(granted, scope)) {
			throw RequestError("user didn't grant necessary scopes");
		}
	}
}
