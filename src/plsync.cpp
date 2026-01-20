#include <cstring>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <string>
#include <vector>
#include "../include/init.h"

const std::vector<const char *> COMMANDS = {"init"};

void print_init_only_usage() {
	std::cout << "usage: plsync [-h] <command>\n\n" 
			  << "Avaliable commands:\n" 
			  << "  init  Allow OAuth permissions for Youtube and Spotify. Initally the only valid command"
			  << std::endl;
}

void print_usage() {
}

const char *parse_args_init_only(int argc, char *argv[]) {
	if (argc < 2 || strcmp(argv[1], "-h")==0 || strcmp(argv[1], "--help")==0) {
		print_init_only_usage();
		exit(1);
	}
	if (strcmp(argv[1], "init") != 0) {
		print_init_only_usage();
		exit(1);
	}
	return argv[1];
}

int main(int argc, char *argv[]) {
	const char *command = parse_args_init_only(argc, argv);
	run_init();
	return 0;
}
