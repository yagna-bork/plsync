#include "../include/util.h"
#include <string>
#include <openssl/sha.h>

std::string sha256(const std::string &s) {
	unsigned char digest[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, s.c_str(), s.size());
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
std::string base64url_encode(const std::string &s, bool pad) {
	std::string encoded;
	unsigned short buffer = 0; // 2 char buffer
	int unread_bits = 0;
	unsigned char b64_mask = 0x3F;
	unsigned char b64_bits = 6;
	unsigned char b64_val, b64_char; 

	for (unsigned char c: s) {
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
	if (s.size() % 3 == 1) {
		encoded.push_back('=');
		encoded.push_back('=');
	} else if (s.size() % 3 == 2) {
		encoded.push_back('=');
	}
	return encoded;
}

std::vector<std::string> split(const std::string &s, const std::string &ss) {
	std::vector<std::string> res;
	std::string::const_iterator j, i = s.cbegin();
	while(j != s.end()) {
		j = find_range(i, s.cend(), ss.cbegin(), ss.cend());
		res.emplace_back(i, j);
		i = j + ss.size();
	}
	return res;
}
