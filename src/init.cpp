#include "../include/init.h"
#include "../include/config.h"
#include "../include/httplib.h"
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
#include <openssl/sha.h>
#include <iterator>

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

std::string sha256(const std::string &str) {
	unsigned char digest[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
	SHA256_Final(digest, &sha256);
	return std::string(digest, digest+SHA256_DIGEST_LENGTH);
}

char base64_char(int value) {
	if (value < 26) {
		return 'A' + value;
	} else if (value < 52) {
		return 'a' + (value - 26);
	} else if (value < 62) {
		return '0' + (value - 52);
	} else if (value == 62) {
		return '-';
	} else {
		return '_';
	}
}

// https://en.wikipedia.org/wiki/Base64
std::string base64url_encode(const std::string &str, bool pad = false) {
	std::string encoded;
	unsigned short buffer = 0; // 2 char buffer
	int unread_bits = 0;
	unsigned char b64_mask = 0x3F;
	unsigned char b64_bits = 6;
	unsigned char b64_val, b64_char; 

	for (unsigned char c: str) {
		buffer = (buffer << 8) + c;
		unread_bits += 8;
		while (unread_bits >= b64_bits) {
			b64_val = (buffer >> (unread_bits - 6)) & b64_mask;
			encoded.push_back(base64_char(b64_val));
			unread_bits -= b64_bits;
		}
	}
	if (unread_bits) {
		unsigned char mask = 0xFF >> (8 - unread_bits);
		b64_val = (buffer & mask); 
		// make it b64_bits long by padding with 0s
		b64_val <<= (6 - unread_bits); 
		encoded.push_back(base64_char(b64_val));
	}

	// padding
	if (!pad) {
		return encoded;
	}
	if (str.size() % 3 == 1) {
		encoded.push_back('=');
		encoded.push_back('=');
	} else if (str.size() % 3 == 2) {
		encoded.push_back('=');
	}
	return encoded;
}

std::string::const_iterator find_str(
	const std::string &str, const std::string &substr, std::string::const_iterator pos
) {
	size_t m = str.size(), n = substr.size();
	if (n > m)
		return str.end();
	size_t j;
	for (auto i = pos; i <= str.end()-n; i++, j=0) {
		while ((j < n) && (*(i+j) == substr[j]))
			j++;
		if (j == n)
			return i;
	}
	return str.end();
}

std::vector<std::string> split(const std::string &str, const std::string &substr) {
	std::vector<std::string> res;
	std::string::const_iterator i = str.begin();
	std::string::const_iterator j = find_str(str, substr, str.begin());
	while(j != str.end()) {
		res.push_back(std::string(i, j));
		i = j + substr.size();
		j = find_str(str, substr, i);
	}
	res.push_back(std::string(i, j));
	return res;
}

void auth_resp_callback(const httplib::Request &req, httplib::Response &res) {
	if (req.has_param("error")) {
		res.set_content("Something went wrong. Please return to terminal and try again", "text/plain");
		res.status = httplib::StatusCode::InternalServerError_500;
		std::cerr << "Failure. Please restart" << std::endl;
	}

	std::vector<std::string> scopes = split(get_setting("youtube_scopes"), ",");
	std::vector<std::string> permitted = split(req.get_param_value("scope"), " ");
	for (const std::string &scope: scopes) {
		if (std::find(permitted.begin(), permitted.end(), scope) != permitted.end()) {
			continue;
		}
		res.set_content(
			"'" + scope + "' scope is required. Please return to terminal and try again", 
			"text/plain"
		);
		res.status = httplib::StatusCode::InternalServerError_500;
		std::cerr << "Failure. Please terminate and try again" << std::endl;
	}

	res.set_content("Success! Return to the terminal", "text/plain");
	res.status = httplib::StatusCode::OK_200;
	std::cerr << "Success! Please terminate and continue" << std::endl;
}

bool get_youtube_auth_code(std::string &code) {
	int status, listenfd, sockfd, yes = 1;
	struct addrinfo hints, *svr_info, *p;
	struct sockaddr_storage client_addr;
	socklen_t addr_len;
	size_t buff_sz = 1024;
	char buff[buff_sz];
	std::string res;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // IPv4 or IPv6, whatever auth server decides
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // figure out host ip for me
	std::string port = get_setting("auth_redirect_port");

	status = getaddrinfo(NULL, port.c_str(), &hints, &svr_info);
	if (status != 0) {
		std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
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
		std::cerr << "Couldn't connect listener socket" << std::endl;
		return false;
	}
	if (listen(listenfd, SERVER_BACKLOG) == -1) {
		std::cerr << "Couldn't listen for connections" << std::endl;
		return false;
	}
	
	sockfd = accept(listenfd, (struct sockaddr *)&client_addr, &addr_len);
	if (sockfd == -1) {
		std::cerr << "Server crashed" << std::endl;
		return false;
	}

	// stuff those pesky params into a map
	size_t bytes_recv = recv(sockfd, buff, buff_sz, 0);
	std::unordered_map<std::string, std::string> params;
	std::string param, val;
	char *beg = std::find(buff, buff+bytes_recv, '?') + 1;
	char *path_end = std::find(beg, buff+bytes_recv, ' ');
	char *amp, *sep;
	do {
		amp = std::find(beg, path_end, '&');
		sep = std::find(beg, amp, '=');
		param = std::string(beg, sep);
		val = std::string(sep+1, amp);
		params[param] = val;
		beg = amp + 1;
	} while (amp != path_end);

	if (params.count("error")) {
		res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nError occured: " 
						  + params["error"] + ". Please return to terminal and try again";
		send(sockfd, res.c_str(), res.size(), 0);
		std::cerr << params["error"] << std::endl;
		return false;
	}
	
	std::vector<std::string> scopes = split(get_setting("youtube_scopes"), ",");
	std::vector<std::string> permitted = split(params["scope"], "%20");
	for (const std::string &scope: scopes) {
		if (std::find(permitted.begin(), permitted.end(), scope) != permitted.end()) {
			continue;
		}
		res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nRequired scopes not granted. " 
						  "Please return to terminal and try again";
		send(sockfd, res.c_str(), res.size(), 0);
		std::cerr << "Scope '" + scope + "' required" << std::endl;
		return false;
	}
	res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nSuccess! Return to terminal to continue";
	send(sockfd, res.c_str(), res.size(), 0);
	code = params["code"];
	return true;
}

int run_init() {
	std::string verifier = generate_code_verifier();
	std::string challenge = base64url_encode(sha256(verifier));

	std::string client_id = get_setting("youtube_client_id");
	std::string redirect_url = get_setting("auth_redirect_url");
	std::string redirect_port = get_setting("auth_redirect_port");
	std::string scopes = get_setting("youtube_scopes");
	std::string auth_url = get_setting("youtube_auth_url");
	std::string full_redirect_url = "http://" + redirect_url + ":" + redirect_port;
	std::replace(scopes.begin(), scopes.end(), ',', '+');

	// TODO make more platform independent
	char command[512];
	snprintf(command, 500, "open '%s?client_id=%s&redirect_uri=%s&response_type=code"
				  	   	   "&scope=%s&code_challenge=%s&code_challenge_method=S256'", 
			auth_url.c_str(), client_id.c_str(), full_redirect_url.c_str(), 
			scopes.c_str(), challenge.c_str());
	system(command);

	std::cout << "Waiting for Youtube authentication code..." << std::endl;
	std::string auth_code;
	if (!get_youtube_auth_code(auth_code)) {
		std::cout << "Unable to complete Youtube OAuth flow. Please try again" << std::endl;
		return 1;
	}
	return 0;
}
