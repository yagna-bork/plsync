#include "../include/util.h"
#include <cassert>
#include <string>
#include <random>
#include <fstream>
#include <filesystem>
#include <openssl/evp.h>

// https://docs.openssl.org/master/man7/ossl-guide-libcrypto-introduction/#using-algorithms-in-applications
bool sha256(const std::string &s, std::string &res) {
	std::unique_ptr<EVP_MD_CTX, void(&)(EVP_MD_CTX*)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) {
		return false;
	}
    std::unique_ptr<EVP_MD, void(&)(EVP_MD*)> sha256(EVP_MD_fetch(nullptr, "SHA256", nullptr), EVP_MD_free);
    if (!sha256) {
		return false;
	}
	if (!EVP_DigestInit_ex(ctx.get(), sha256.get(), nullptr)) {
		return false;
	}
    if (!EVP_DigestUpdate(ctx.get(), s.data(), s.size())) {
		return false;
	}

	int digest_len = EVP_MD_get_size(sha256.get());
	unsigned char digest[digest_len];
    if (!EVP_DigestFinal_ex(ctx.get(), digest, nullptr)) {
		return false;
	}

	size_t pad_len = std::max(size_t(0), digest_len - res.size());
	std::fill_n(std::back_inserter(res), pad_len, 0);
	std::copy(digest, digest+digest_len, res.begin());
	return true;
}

char getb64char(int value) {
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
std::string urlencode64(const std::string &s, bool pad) {
	std::string encoded;
	unsigned short buffer = 0; // 2 char buffer
	int unread_bits = 0;
	unsigned char b64mask = 0x3F;
	unsigned char b64bits = 6;
	unsigned char b64val, b64char; 

	for (unsigned char c: s) {
		buffer = (buffer << 8) + c;
		unread_bits += 8;
		while (unread_bits >= b64bits) {
			b64val = (buffer >> (unread_bits - 6)) & b64mask;
			encoded.push_back(getb64char(b64val));
			unread_bits -= b64bits;
		}
	}
	if (unread_bits) {
		unsigned char mask = 0xFF >> (8 - unread_bits);
		b64val = (buffer & mask); 
		// make it b64bits long by padding with 0s
		b64val <<= (6 - unread_bits); 
		encoded.push_back(getb64char(b64val));
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

std::string join(const std::vector<std::string> &strs, const std::string &sep) {
	std::string joined;
	for (std::size_t i = 0; i != strs.size(); i++) {
		if (i != 0) {
			joined.append(sep);
		}
		joined.append(strs[i]);
	}
	return joined;
}

bool contains(const std::string &s, const std::string &ss) {
	return find_range(s.begin(), s.end(), ss.begin(), ss.end()) != s.end();
}

size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *data_p) {
	std::string *data = static_cast<std::string *>(data_p);
	copy(ptr, ptr + nmemb, back_inserter(*data));
	return nmemb;
}

size_t curl_fwrite_cb(char *st, size_t size, size_t nmemb, void *file_p) {
	std::ofstream *file = static_cast<std::ofstream *>(file_p);
	size_t nbyte = size * nmemb;
	return file->write(st, nbyte) ? nbyte : 0;
}

std::string rndstr(size_t size) {
	assert(size % 8 == 0);
	std::string res;
	res.reserve(size);
	std::random_device rd;
	std::mt19937_64 gen64(rd());
	unsigned long long rnd_num;
	unsigned char byte;

	
	for (int i = 0; i != (size / 8); i++) {
		rnd_num = gen64();
		for (int j = 0; j != 8; j++) {
			byte = static_cast<unsigned char>(rnd_num);
			res.push_back(byte);
			rnd_num >>= 8;
		}
	}
	return res;
}

bool ensure_tmpdir(std::filesystem::path &tmpdir) {
	tmpdir = std::filesystem::temp_directory_path() / "plsync";
	if (std::filesystem::exists(tmpdir)) {
		return true;
	}
	if (!std::filesystem::create_directory(tmpdir)) {
		tmpdir.clear();
		return false;
	}
	return true;
}

// nibble = half a byte hehe
char hex_bit(char nibble) {
	if (nibble <= 9) {
		return '0' + nibble;
	} else {
		return 'a' + nibble - 10;
	}
}

std::string bin_to_hex(const std::string& data) {
	std::string res;
	for (unsigned char byte: data) {
		res.push_back(hex_bit(byte >> 4)); // most significant nibble 
		res.push_back(hex_bit(byte & 15)); // least significant nibble
	}
	return res;
}
