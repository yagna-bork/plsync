#include <fstream>
#include <iostream>
#include <ios>
#include <utf8proc.h>

int main() {
	std::ifstream file("emojis.txt");
	utf8proc_int32_t cp;
	std::string st;
	size_t wd; 
	std::cout.setf(std::ios_base::hex, std::ios_base::basefield);
	file.setf(std::ios_base::hex, std::ios_base::basefield);
	while (!file.eof()) {
		file >> cp;
		file >> st;
		size_t wd = utf8proc_charwidth(cp);
		bool fail = ((wd == 1) && (st == "emoji")) || ((wd == 2) && (st == "text"));
		if (fail) {
			std::cout << cp << '\n';
		}
	}
	return 0;
}
