#include "../include/sync.h"
#include <iostream>

static void print_usage() {
	std::cout << "usage: plsync sync\n\n" << sync_description << '\n';
}

static bool parse_args(int argc, char* argv[]) {
	if (argc > 0) {
		print_usage();
		return false;
	}
	return true;
}

int run_sync(int argc, char* argv[]) {
	if (!parse_args(argc, argv)) {
		return 0;
	}
	std::cout << "Syncing...";
	std::cout << " Done!\n";
	return 0;
}
