#include "../include/init.h"
#include "../include/token_refresher.h"
#include "../include/untracked.h"
#include <cstring>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <string>
#include <vector>

const std::vector<const char *> COMMANDS = {"init", "untracked"};

void print_init_only_usage() {
	std::cout << "usage: plsync [-h] <command>\n\n" 
			  << "Avaliable commands:\n" 
			  << "  init  Allow OAuth permissions for Youtube and Spotify. Initially the only valid command"
			  << std::endl;
}

void print_usage() {
	std::cout << "usage: plsync [-h] <command>\n\n" 
			  << "Avaliable commands:\n" 
			  << "  init       Allow OAuth permissions for Youtube and Spotify. Initially the only valid command\n\n"
			  << "  untracked  " << untracked::description
			  << '\n';
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

const char *parse_args(int argc, char *argv[]) {
	if (argc < 2 || strcmp(argv[1], "-h")==0 || strcmp(argv[1], "--help")==0) {
		print_usage();
		exit(1);
	}
	for (const char *cmd: COMMANDS) {
		if (strcmp(cmd, argv[1]) == 0) {
			return cmd;
		}
	}
	print_usage();
	exit(1);
}

int main(int argc, char *argv[]) {
	YoutubeTokenRefresher ytref;
	bool yt_valid = ytref.refresh_tkn_valid();
	SpotifyTokenRefresher spref;
	bool sp_valid = spref.refresh_tkn_valid();
	if (!yt_valid || !sp_valid) {
		parse_args_init_only(argc, argv);
		return run_init(yt_valid, sp_valid);
	}

	const char *command = parse_args(argc, argv);
	if (strcmp(command, "init") == 0) {
		return run_init(true, true);
	} else if (strcmp(command, "untracked") == 0) {
		return run_untracked(argc-2, argv+2);
	}
	return 0;
}
