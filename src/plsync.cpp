#include "../include/init.h"
#include "../include/token_store.h"
#include "../include/untracked.h"
#include "../include/platform.h"
#include "../include/track.h"
#include "../include/tracked.h"
#include "../include/untrack.h"
#include "../include/sync.h"
#include <cstring>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <string>
#include <vector>

const std::vector<const char *> COMMANDS = {"init", "untracked", "track", "tracked", "untrack", "sync"};

static void print_init_only_usage() {
	std::cout << "usage: plsync [-h] <command>\n\n" 
			  << "Avaliable commands:\n" 
			  << "  init  Allow OAuth permissions for Youtube and Spotify. Initially the only valid command"
			  << std::endl;
}

static void print_usage() {
	std::cout << "usage: plsync [-h] <command>\n\n" 
			  << "Avaliable commands:\n" 
			  << "  init       " << init::description << "\n\n"
			  << "  untracked  " << untracked::description << "\n\n"
			  << "  track      " << track_description << "\n\n"
			  << "  tracked    " << tracked_description << "\n\n"
			  << "  untrack    " << untrack_description << "\n\n"
			  << "  sync       " << sync_description << '\n';
}

static const char *parse_args_init_only(int argc, char *argv[]) {
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

static const char *parse_args(int argc, char *argv[]) {
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
	bool is_yt_init = is_refresh_tkn_valid(Platform::YOUTUBE);
	bool is_sp_init = is_refresh_tkn_valid(Platform::SPOTIFY);
	if (!is_yt_init || !is_sp_init) {
		parse_args_init_only(argc, argv);
		return run_init(!is_yt_init, !is_sp_init);
	}

	// TODO reduce the number of files by putting all
	// seperate command files into this single file
	const char *command = parse_args(argc, argv);
	if (strcmp(command, "init") == 0) {
		return run_init(true, true);
	} else if (strcmp(command, "untracked") == 0) {
		return run_untracked(argc-2, argv+2);
	} else if (strcmp(command, "track") == 0) {
		return run_track(argc-2, argv+2);
	} else if (strcmp(command, "tracked") == 0) {
		return run_tracked(argc-2, argv+2);
	} else if (strcmp(command, "untrack") == 0) {
		return run_untrack(argc-2, argv+2);
	} else if (strcmp(command, "sync") == 0) {
		return run_sync(argc-2, argv+2);
	}
	return 0;
}
