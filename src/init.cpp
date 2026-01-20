#include "../include/init.h"
#include <iostream>
#include <random>
#include <string>
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
std::string base64url_encode(const std::string &str) {
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
	if (str.size() % 3 == 1) {
		encoded.push_back('=');
		encoded.push_back('=');
	} else if (str.size() % 3 == 2) {
		encoded.push_back('=');
	}
	return encoded;
}

void run_init() {
	std::string verifier = generate_code_verifier();
	std::string challenge = base64url_encode(sha256(verifier));
}
