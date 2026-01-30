#ifndef GUARD_UTIL_H
#define GUARD_UTIL_H
#include <cstdio>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <zlib.h>

/*
 * A collection of functions used by this project
 * that could be useful for other projects too.
 */

bool sha256(const std::string &s, std::string &digest);

std::string urlencode64(const std::string &s, bool pad = false);

// TODO implement KMF
template <class ForwardIt>
ForwardIt find_range(ForwardIt first1, ForwardIt last1, ForwardIt first2, ForwardIt last2) {
	ForwardIt j, k;
	for (ForwardIt i = first1; i != last1; i++) {
		j = i;
		k = first2;
		while(k != last2 && j != last1 && *j == *k) {
			j++;
			k++;
		}
		if (k == last2) {
			return i;
		}
	}
	return last1;
}

std::vector<std::string> split(const std::string &st, const std::string &subst);

/* Pass to curl WRITE_FUNCTION opt */
size_t curl_write_cb(char *, size_t, size_t, void *);

size_t curl_fwrite_cb(char *, size_t, size_t, void *);

/* 
 * RAII alternative to curl string lists which can
 * be used to add headers to a request
 */
class curl_slist_raii {
public:
	curl_slist_raii(): p(nullptr) {}
	
	// no copy or assignment
	curl_slist_raii(const curl_slist_raii&) = delete;
	curl_slist_raii &operator=(const curl_slist_raii&) = delete;

	void append(const char *s) { p = curl_slist_append(p, s); }
	void append(const std::string &s) { append(s.c_str()); }
	
	struct curl_slist *get() { return p; }

	~curl_slist_raii() { curl_slist_free_all(p); }

private:
	struct curl_slist *p;
};

struct gzDeleter {
	void operator()(gzFile f) { gzclose(f); }
};

struct fileDeleter {
	void operator()(std::FILE *f) { std::fclose(f); }
};

/* size must be a multiple of 8 */
std::string rndstr(size_t size);
#endif
