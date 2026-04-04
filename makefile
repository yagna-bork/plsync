CXX = -std=c++20
VCPKG = $(HOME)/Applications/vcpkg/installed/x64-osx-dynamic/lib
LIBS = -L$(VCPKG) -lcred `pkg-config --libs openssl libcurl zlib protobuf libutf8proc`
OBJS = obj/util.o obj/platform.o obj/api.o obj/youtube_api.o obj/spotify_api.o obj/new_api.o obj/cache.pb.o obj/cache.o
NDEBUG = -D NDEBUG
DEBUG_SYM = -g -O0
# TODO in prod enable NDEBUG to ignore asserts and ignore DEBUG_SYM
DEBUG_OR_PROD = $(DEBUG_SYM)


# TODO for prod need to make vcpkg install within project
plsync bin/plsync: obj/plsync.o $(OBJS) bin/make_emoji_codepoint_ranges_h
	./bin/make_emoji_codepoint_ranges_h
	clang++ -o bin/plsync $(LIBS) obj/plsync.o $(OBJS) $(DEBUG_OR_PROD)
	install_name_tool -change @rpath/libcred.1.dylib $(VCPKG)/libcred.1.dylib bin/plsync
	chmod u+x bin/plsync

all:
	make clean
	make plsync
	make tests

clean:
	rm -rf obj bin
	mkdir obj bin


# object files
obj/plsync.o: src/plsync.cpp include/api.h include/platform.h include/util.h include/new_api.h include/cache.pb.h include/cache.h
	clang++ -o obj/plsync.o -c $(CXX) src/plsync.cpp $(DEBUG_OR_PROD)

obj/util.o: src/util.cpp include/util.h include/api.h
	clang++ -o obj/util.o -c $(CXX) src/util.cpp $(DEBUG_OR_PROD)

obj/platform.o: src/platform.cpp include/platform.h
	clang++ -o obj/platform.o -c $(CXX) src/platform.cpp $(DEBUG_OR_PROD)

obj/api.o: src/api.cpp include/api.h include/util.h include/platform.h
	clang++ -o obj/api.o -c $(CXX) src/api.cpp $(DEBUG_OR_PROD)

obj/youtube_api.o: src/youtube_api.cpp include/youtube_api.h include/platform.h include/api.h
	clang++ -o obj/youtube_api.o -c $(CXX) src/youtube_api.cpp $(DEBUG_OR_PROD)

obj/spotify_api.o: src/spotify_api.cpp include/spotify_api.h include/platform.h include/api.h
	clang++ -o obj/spotify_api.o -c $(CXX) src/spotify_api.cpp $(DEBUG_OR_PROD)

obj/cache.pb.o: src/cache.pb.cc include/cache.pb.h
	clang++ -o obj/cache.pb.o -c $(CXX) src/cache.pb.cc $(DEBUG_OR_PROD)

obj/new_api.o: src/new_api.cpp include/new_api.h include/cache.h include/platform.h include/util.h
	clang++ -o obj/new_api.o -c $(CXX) src/new_api.cpp $(DEBUG_OR_PROD)

obj/cache.o: include/cache.h src/cache.cpp include/util.h include/cache.pb.h include/new_api.h
	clang++ -o obj/cache.o -c $(CXX) src/cache.cpp $(DEBUG_OR_PROD)


# scripts
bin/make_emoji_codepoint_ranges_h: scripts/make_emoji_codepoint_ranges_h.cpp
	clang++ -o bin/make_emoji_codepoint_ranges_h scripts/make_emoji_codepoint_ranges_h.cpp $(CXX)


# tests
# TODO for prod need to make vcpkg install within project
tests bin/test: test/* $(OBJS)
	clang++ -o bin/test test/test.cpp $(CXX) $(LIBS) $(OBJS) $(DEBUG_OR_PROD)
	install_name_tool -change @rpath/libcred.1.dylib $(VCPKG)/libcred.1.dylib bin/test
