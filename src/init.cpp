#include "../include/init.h"
#include "../include/config.h"
#include "../include/httplib.h"
#include "../include/util.h"
#include <ctime>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>
#include <random>
#include <string>
#include <iterator>
#include <utility>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <libcred.hpp>

const size_t BUFFSZ = 1024;
char BUFF[BUFFSZ];
const int SERVER_BACKLOG = 5;

std::string generate_code_verifier() {
	std::string res;
	res.reserve(128);
	std::random_device rd;
	std::mt19937_64 gen64(rd());

	unsigned long long rnd_num;
	unsigned char byte;
	// between 0-65 and represents a char allowed in verifier
	int char_opt; 
	for (int i = 0; i != 128; i+=8) {
		rnd_num = gen64();

		for (int j = 0; j != 8; j++) {
			byte = static_cast<unsigned char>(rnd_num);
			char_opt = byte % 66;

			if (char_opt < 26) { // A-Z
				res.push_back('A' + char_opt);
			} else if (char_opt < 52) { // a-z
				res.push_back('a' + (char_opt - 26));
			} else if (char_opt < 62) { // 0-9
				res.push_back('0' + (char_opt - 52));
			} else if (char_opt == 62) {
				res.push_back('-');
			} else if (char_opt == 63) {
				res.push_back('.');
			} else if (char_opt == 64) {
				res.push_back('_');
			} else {
				res.push_back('~');
			}
			rnd_num >>= 8;
		}
	}
	return res;
}

std::string generate_state() {
	size_t state_sz = 128;
	std::string state;
	state.reserve(state_sz);
	std::random_device rd;
	std::mt19937_64 gen64(rd());
	unsigned long long rnd_num;
	unsigned char byte;

	for (int i = 0; i != (state_sz / 8); i++) {
		rnd_num = gen64();
		for (int j = 0; j != 8; j++) {
			byte = static_cast<unsigned char>(rnd_num);
			state.push_back(byte);
			rnd_num >>= 8;
		}
	}
	return base64url_encode(state);
}

bool get_auth_code(std::string &code, const std::string &state, const std::string &port) {
	int status, listenfd, sockfd, yes = 1;
	struct addrinfo hints, *svr_info, *p;
	struct sockaddr_storage client_addr;
	socklen_t addr_len;
	std::string res;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // IPv4 or IPv6, whatever auth server decides
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // figure out host ip for me

	status = getaddrinfo(NULL, port.c_str(), &hints, &svr_info);
	if (status != 0) {
		std::cerr << std::endl << "Couldn't get host network information: " << gai_strerror(status) << std::endl;
		return false;
	}

	for (p = svr_info; p != nullptr; p = p->ai_next) {
		listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listenfd == -1) {
			continue;
		}
		status = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
		if (status == -1) {
			continue;
		}
		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == -1) {
			continue;
		}
		break;
	}

	if (p == nullptr) {
		std::cerr << std::endl << "Couldn't connect listener socket" << std::endl;
		return false;
	}
	if (listen(listenfd, SERVER_BACKLOG) == -1) {
		std::cerr << std::endl << "Couldn't listen for connections" << std::endl;
		return false;
	}
	
	sockfd = accept(listenfd, (struct sockaddr *)&client_addr, &addr_len);
	close(listenfd);
	if (sockfd == -1) {
		std::cerr << std::endl << "Server crashed" << std::endl;
		return false;
	}

	// stuff those pesky params into a map
	size_t bytes_recv = recv(sockfd, BUFF, BUFFSZ, 0);
	std::unordered_map<std::string, std::string> params;
	std::string param, val;
	char *beg = std::find(BUFF, BUFF+bytes_recv, '?') + 1;
	char *end = std::find(beg, BUFF+bytes_recv, ' ');
	char *amp, *eq;
	do {
		amp = std::find(beg, end, '&');
		eq = std::find(beg, amp, '=');
		param = std::string(beg, eq);
		val = std::string(eq+1, amp);
		params[param] = val;
		beg = amp + 1;
	} while (amp != end);

	if (params.count("error")) {
		res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nError occured: " 
						  + params["error"] + ". Please return to terminal and try again";
		send(sockfd, res.c_str(), res.size(), 0);
		std::cerr << std::endl << params["error"] << std::endl;
		return false;
	}

	if (!params.count("state") || params["state"] != state) {
		std::cout << "Blocked cross-site request forgery attempt" << std::endl;
		return false;
	}
	res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nSuccess! Return to terminal to continue";
	send(sockfd, res.c_str(), res.size(), 0);
	close(sockfd);
	code = params["code"];
	return true;
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *data) {
	std::string *datap = (std::string *)data;
	copy(ptr, ptr + nmemb, back_inserter(*datap));
	return nmemb;
}

/*
 * Monsterous parameter list ik ik. 
 * Will have to refactor somehow.
 */
bool get_access_tokens(
	const std::string &url, const std::string &client_id, 
	const std::string &auth_code, const std::string &verifier, const std::string &redirect_url, 
	const std::string &client_secret, const std::string &platform, const std::string &scope,
	std::string &access_tkn, std::string &access_expiry, 
	std::string &refresh_tkn, std::string &refresh_expiry
) {
	std::string res;
	CURL *handle = curl_easy_init();
	if (!handle) {
		std::cerr << "Failed to setup easy curl" << std::endl;
		return false;
	}

	int write_sz = snprintf(
		BUFF, BUFFSZ, "client_id=%s&code=%s&code_verifier=%s&grant_type=authorization_code&"
					 "redirect_uri=%s",
		client_id.c_str(), auth_code.c_str(), verifier.c_str(), 
		redirect_url.c_str()
	);
	if (!client_secret.empty()) {
		snprintf(BUFF+write_sz, BUFFSZ-write_sz, "&client_secret=%s", client_secret.c_str());
	}

	curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &res);
	curl_easy_setopt(handle, CURLOPT_POST, 1);
	curl_easy_setopt(handle, CURLOPT_POSTFIELDS, BUFF);

	CURLcode status = curl_easy_perform(handle);
	curl_easy_cleanup(handle);
	if(status != CURLE_OK) {
		std::cerr << "Failed to access " << platform << " auth server" << std::endl;
		return false;
	}

	nlohmann::json jres = nlohmann::json::parse(res, /*cb=*/nullptr, /*allow_exceptions=*/false);
	if (jres.is_discarded()) {
		std::cerr << "Couldn't process " << platform << " authentication token response\n";
		return false;
	}

	std::vector<std::string> scopes = split(scope, ",");
	std::vector<std::string> permitted = split(jres["scope"], " ");
	for (const std::string &sc: scopes) {
		if (std::find(permitted.begin(), permitted.end(), sc) != permitted.end()) {
			continue;
		}
		std::cerr << std::endl << "Scope '" + sc + "' required" << std::endl;
		return false;
	}

	access_tkn = jres["access_token"];
	access_expiry = std::to_string(time(nullptr) + jres.value<long>("expires_in", 0));
	refresh_tkn = jres["refresh_token"];
	refresh_expiry = "-1";
	if (jres.contains("refresh_token_expires_in")) {
		refresh_expiry = std::to_string(
			time(nullptr) + jres.value<long>("refresh_token_expires_in", 0)
		);
	}
	return true;
}

bool get_user_permissions(
	std::string platform, const std::string &client_id, const std::string &scopes, 
	const std::string &auth_url, const std::string &access_tkn_url, 
	const std::string &redirect_port, const std::string client_secret = ""
) {
	std::string verifier = generate_code_verifier();
	std::string challenge = base64url_encode(sha256(verifier));
	std::string state = generate_state();
	std::string redirect_url = get_setting("auth_redirect_url");
	std::string full_redirect_url = redirect_url + ":" + redirect_port;
	platform[0] = toupper(platform[0]);

	std::string fmt_scopes;
	std::replace_copy(scopes.begin(), scopes.end(), std::back_inserter(fmt_scopes), ',', '+');
	// TODO make more platform independent
	snprintf(BUFF, BUFFSZ, 
		"open '%s?client_id=%s&redirect_uri=%s&response_type=code"
		"&scope=%s&code_challenge=%s&code_challenge_method=S256&state=%s'", 
		auth_url.c_str(), client_id.c_str(), full_redirect_url.c_str(), fmt_scopes.c_str(), 
		challenge.c_str(), state.c_str()
	);
	system(BUFF);

	std::cout << "Waiting for " << platform << " authentication code... ";
	std::string auth_code;
	if (!get_auth_code(auth_code, state, redirect_port)) {
		std::cout << "Unable to complete " << platform << " authentication. Please try again" << std::endl;
		return false;
	}
	std::cout << "Got it!\n";

	std::string access_tkn, access_expiry, refresh_tkn, refresh_expiry;
	bool success = get_access_tokens(
		access_tkn_url, client_id, auth_code, verifier, full_redirect_url, 
		client_secret, platform, scopes, access_tkn, access_expiry, refresh_tkn, refresh_expiry
	);
	if (!success) {
		std::cerr << "Something went wrong. Please try again" << std::endl;
		return false;
	}

	// now store the tokens
	platform[0] = tolower(platform[0]);
	std::string err;
	libcred::LIBCRED_RESULT status = libcred::set_password(
		"plsync-token-service", platform+"-access-token", access_tkn + ":" + access_expiry, &err
	);
	if (status != libcred::LIBCRED_RESULT::SUCCESS) {
		std::cerr << "Couldn't store tokens in keychain. Please try again" << std::endl;
		return false;
	}

	status = libcred::set_password(
		"plsync-token-service", platform+"-refresh-token", refresh_tkn + ":" + refresh_expiry, &err
	);
	if (status != libcred::LIBCRED_RESULT::SUCCESS) {
		std::cerr << "Couldn't store tokens in keychain. Please try again" << std::endl;
		return false;
	}
	platform[0] = toupper(platform[0]);
	std::cout << "Success! " << platform << " authentication completed" << std::endl;
	return true;
}

int run_init() {
	bool success = get_user_permissions(
		"youtube", get_setting("yt_client_id"), get_setting("yt_scopes"), get_setting("yt_auth_url"), 
		get_setting("yt_access_tkn_url"), get_setting("yt_redirect_port"), get_setting("yt_client_secret")
	);
	if (!success) {
		return 1;
	}
	success = get_user_permissions(
		"spotify", get_setting("sp_client_id"), get_setting("sp_scopes"), get_setting("sp_auth_url"), 
		get_setting("sp_access_tkn_url"), get_setting("sp_redirect_port")
	);
	if (!success) {
		return 1;
	}
	return 0;
}
