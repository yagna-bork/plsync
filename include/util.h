#ifndef GUARD_UTIL_H
#define GUARD_UTIL_H
#include <string>
#include <vector>

/*
 * A collection of functions used by this project
 * that could be useful for other projects too.
 */

std::string sha256(const std::string &s);

std::string base64url_encode(const std::string &s, bool pad = false);

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
size_t write_callback(char *, size_t, size_t, void *);
#endif
