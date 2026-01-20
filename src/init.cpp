#include "../include/init.h"
#include <iostream>
#include <random>
#include <string>
#include <sstream>
#include <iomanip>
#include <ios>
#include <openssl/sha.h>

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
				res.push_back('a' + char_opt);
			} else if (char_opt < 62) { // 0-9
				res.push_back('0' + char_opt);
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

	std::stringstream ss;
	ss << std::hex << std::setfill('0');
	for (int i = 0; i != SHA256_DIGEST_LENGTH; i++) {
		ss << static_cast<int>(digest[i]);
	}
	return ss.str();
}

void run_init() {
	std::string verifier = generate_code_verifier();
	std::string challenge = sha256(verifier);
}
