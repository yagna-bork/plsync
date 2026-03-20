CXX = -std=c++17
VCPKG = $(HOME)/Applications/vcpkg/installed/x64-osx-dynamic/lib
LIBS = -L$(VCPKG) -lcred `pkg-config --libs openssl libcurl zlib protobuf`
OBJS = obj/init.o obj/config.o obj/util.o \
			obj/token_store.o obj/platform.o \
			obj/untracked.o obj/api.o obj/youtube_api.o obj/spotify_api.o \
			obj/playlist_cache.o obj/playlist_cache.pb.o \
			obj/sid_to_id_map.o obj/sid_to_id_map.pb.o obj/track.o obj/new_api.o \
			obj/new_youtube_api.o obj/new_spotify_api.o obj/playlist_items_cache.pb.o \
			obj/playlist_items_cache.o obj/tracked.o
TEST_OBJS = obj/token_store.o obj/platform.o \
			obj/api.o obj/youtube_api.o obj/spotify_api.o obj/config.o obj/util.o
NDEBUG = -D NDEBUG
DEBUG_SYM = -g -O0

# TODO in prod enable NDEBUG to ignore asserts and ignore DEBUG_SYM
DEBUG_OR_PROD = $(DEBUG_SYM)


# TODO for prod need to make vcpkg install within project

# link everything together
plsync bin/plsync: obj/plsync.o $(OBJS)
	clang++ -o bin/plsync $(LIBS) obj/plsync.o $(OBJS) $(DEBUG_OR_PROD)
	install_name_tool -change @rpath/libcred.1.dylib $(VCPKG)/libcred.1.dylib bin/plsync
	chmod u+x bin/plsync


# object files
obj/plsync.o: src/plsync.cpp include/init.h
	clang++ -o obj/plsync.o -c $(CXX) src/plsync.cpp $(DEBUG_OR_PROD)

obj/init.o: src/init.cpp include/init.h include/config.h include/token_store.h \
			include/util.h include/youtube_api.h include/spotify_api.h include/platform.h
	clang++ -o obj/init.o -c $(CXX) src/init.cpp $(DEBUG_OR_PROD)

obj/config.o: src/config.cpp include/config.h include/platform.h include/client_secret.h
	clang++ -o obj/config.o -c $(CXX) src/config.cpp $(DEBUG_OR_PROD)

obj/util.o: src/util.cpp include/util.h
	clang++ -o obj/util.o -c $(CXX) src/util.cpp $(DEBUG_OR_PROD)

obj/token_store.o: src/token_store.cpp include/token_store.h include/platform.h
	clang++ -o obj/token_store.o -c $(CXX) src/token_store.cpp $(DEBUG_OR_PROD)

obj/platform.o: src/platform.cpp include/platform.h
	clang++ -o obj/platform.o -c $(CXX) src/platform.cpp $(DEBUG_OR_PROD)

obj/untracked.o: src/untracked.cpp include/untracked.h include/playlist_cache.h include/sid_to_id_map.h
	clang++ -o obj/untracked.o -c $(CXX) src/untracked.cpp $(DEBUG_OR_PROD)
	
obj/api.o: src/api.cpp include/api.h include/util.h
	clang++ -o obj/api.o -c $(CXX) src/api.cpp $(DEBUG_OR_PROD)

obj/youtube_api.o: src/youtube_api.cpp include/youtube_api.h include/platform.h include/api.h
	clang++ -o obj/youtube_api.o -c $(CXX) src/youtube_api.cpp $(DEBUG_OR_PROD)

obj/spotify_api.o: src/spotify_api.cpp include/spotify_api.h include/platform.h include/api.h
	clang++ -o obj/spotify_api.o -c $(CXX) src/spotify_api.cpp $(DEBUG_OR_PROD)

obj/playlist_cache.o: src/playlist_cache.cpp include/playlist_cache.h include/models.h include/config.h \
					  include/util.h include/platform.h
	clang++ -o obj/playlist_cache.o -c $(CXX) src/playlist_cache.cpp $(DEBUG_OR_PROD)

obj/playlist_cache.pb.o: include/playlist_cache.pb.h src/playlist_cache.pb.cc
	clang++ -o obj/playlist_cache.pb.o -c $(CXX) src/playlist_cache.pb.cc $(DEBUG_OR_PROD)

obj/cache.pb.o: src/cache.pb.cc include/cache.pb.h
	clang++ -o obj/cache.pb.o -c $(CXX) src/cache.pb.cc $(DEBUG_OR_PROD)

obj/sid_to_id_map.pb.o: include/sid_to_id_map.pb.h 
	clang++ -o obj/sid_to_id_map.pb.o -c $(CXX) src/sid_to_id_map.pb.cc $(DEBUG_OR_PROD)

obj/sid_to_id_map.o: src/sid_to_id_map.cpp include/sid_to_id_map.h include/sid_to_id_map.pb.h \
					 include/util.h include/platform.h include/playlist_cache.h
	clang++ -o obj/sid_to_id_map.o -c $(CXX) src/sid_to_id_map.cpp $(DEBUG_OR_PROD)

obj/track.o: src/track.cpp include/track.h include/playlist_cache.h
	clang++ -o obj/track.o -c $(CXX) src/track.cpp $(DEBUG_OR_PROD)

obj/new_api.o: src/new_api.cpp include/new_api.h
	clang++ -o obj/new_api.o -c $(CXX) src/new_api.cpp $(DEBUG_OR_PROD)

obj/new_youtube_api.o: src/new_youtube_api.cpp include/new_youtube_api.h
	clang++ -o obj/new_youtube_api.o -c $(CXX) src/new_youtube_api.cpp $(DEBUG_OR_PROD)

obj/new_spotify_api.o: src/new_spotify_api.cpp include/new_spotify_api.h
	clang++ -o obj/new_spotify_api.o -c $(CXX) src/new_spotify_api.cpp $(DEBUG_OR_PROD)

obj/playlist_items_cache.pb.o: src/playlist_items_cache.pb.cc include/playlist_items_cache.pb.h
	clang++ -o obj/playlist_items_cache.pb.o -c $(CXX) src/playlist_items_cache.pb.cc $(DEBUG_OR_PROD)

obj/playlist_items_cache.o: src/playlist_items_cache.cpp include/playlist_items_cache.h
	clang++ -o obj/playlist_items_cache.o -c $(CXX) src/playlist_items_cache.cpp $(DEBUG_OR_PROD)

obj/tracked.o: src/tracked.cpp include/tracked.h
	clang++ -o obj/tracked.o -c $(CXX) src/tracked.cpp $(DEBUG_OR_PROD)

# tests
# TODO for prod need to make vcpkg install within project
tests bin/test: test/* $(TEST_OBJS)
	clang++ -o bin/test test/test.cpp $(CXX) $(LIBS) $(TEST_OBJS) $(DEBUG_OR_PROD)
	install_name_tool -change @rpath/libcred.1.dylib $(VCPKG)/libcred.1.dylib bin/test

all:
	make clean
	make plsync
	make tests

clean:
	rm -rf obj bin
	mkdir obj bin
