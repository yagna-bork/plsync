#include "../include/util.h"
#include <iostream>
#include <string>
#include <vector>

namespace TestUtil {
	void test_bin_to_hex() {
		std::vector<std::string> expected = {
			"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0a", "0b", "0c", "0d", "0e", "0f"
		};
		std::vector<std::string> actual;
		for (std::size_t i = 0; i != 16; i++) {
			actual.emplace_back(bin_to_hex(std::string(1, i)));
		}
		if (actual == expected) {
			std::cout << "test_bin_to_hex(): PASSED\n";
		} else {
			std::cout << "test_bin_to_hex(): FAILED\n";
		}
	}

	void run() {
		test_bin_to_hex();
	}
}
