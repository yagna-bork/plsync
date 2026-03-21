#include <fstream>
#include <ios>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>

int main() {
	std::ifstream file("scripts/emojis.txt");
	file.setf(std::ios::hex, std::ios::basefield);
	uint32_t cp;
	std::vector<std::pair<uint32_t, uint32_t>> cp_ranges;
	while (!file.eof()) {
		file >> cp;
		if (!cp_ranges.empty() && (cp_ranges.back().second + 1 == cp)) {
			cp_ranges.back().second = cp;
			continue;
		}
		cp_ranges.push_back({cp, cp});
	}

	std::stringstream array_literal;
	array_literal << std::hex << "{{";
	for (int i = 0; i != cp_ranges.size(); i++) {
		if (i > 0) array_literal << ',';
		array_literal << "{0x" << cp_ranges[i].first << ",0x" << cp_ranges[i].second << '}';
	}
	array_literal << "}}";

	std::ofstream header_file("include/emoji_codepoint_ranges.h");
	header_file << "#ifndef GUARD_EMOJI_CODEPOINT_RANGES_H\n"
				<< "#define GUARD_EMOJI_CODEPOINT_RANGES_H\n"
				<< "#include <array>\n"
				<< "#include <utility>\n\n"
				<< "std::array<std::pair<uint32_t, uint32_t>, " << cp_ranges.size() << "> emoji_cp_ranges = " << array_literal.rdbuf() << ";\n"
				<< "#endif\n";
	return 0;
}
