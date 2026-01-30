CXX = -std=c++17
VCPKG = $(HOME)/Applications/vcpkg/installed/x64-osx-dynamic/lib
LIBS = -L$(VCPKG) -lcred `pkg-config --libs openssl libcurl zlib`
OBJS = obj/init.o obj/config.o obj/util.o \
			obj/token_store.o obj/token_refresher.o obj/platform.o \
			obj/untracked.o
TEST_OBJS = obj/token_store.o obj/token_refresher.o obj/platform.o \
			obj/api.o obj/youtube_api.o obj/config.o obj/util.o
NDEBUG = -D NDEBUG
DEBUG_SYM = -g -O0

# TODO enable NDEBUG to ignore asserts and ignore DEBUG_SYM
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

obj/init.o: src/init.cpp include/init.h include/config.h include/token_store.h include/util.h
	clang++ -o obj/init.o -c $(CXX) src/init.cpp $(DEBUG_OR_PROD)

obj/config.o: src/config.cpp include/config.h include/platform.h include/client_secret.h
	clang++ -o obj/config.o -c $(CXX) src/config.cpp $(DEBUG_OR_PROD)

obj/util.o: src/util.cpp include/util.h
	clang++ -o obj/util.o -c $(CXX) src/util.cpp $(DEBUG_OR_PROD)

obj/token_store.o: src/token_store.cpp include/token_store.h
	clang++ -o obj/token_store.o -c $(CXX) src/token_store.cpp $(DEBUG_OR_PROD)

obj/token_refresher.o: src/token_refresher.cpp include/token_refresher.h include/token_store.h
	clang++ -o obj/token_refresher.o -c $(CXX) src/token_refresher.cpp $(DEBUG_OR_PROD)

obj/platform.o: src/platform.cpp include/platform.h
	clang++ -o obj/platform.o -c $(CXX) src/platform.cpp $(DEBUG_OR_PROD)

obj/untracked.o: src/untracked.cpp include/untracked.h
	clang++ -o obj/untracked.o -c $(CXX) src/untracked.cpp $(DEBUG_OR_PROD)
	
obj/api.o: src/api.cpp include/api.h include/util.h
	clang++ -o obj/api.o -c $(CXX) src/api.cpp $(DEBUG_OR_PROD)

obj/youtube_api.o: src/youtube_api.cpp include/youtube_api.h include/platform.h
	clang++ -o obj/youtube_api.o -c $(CXX) src/youtube_api.cpp $(DEBUG_OR_PROD)

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
