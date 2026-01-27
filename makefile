CXX = -std=c++17
VCPKG = $(HOME)/Applications/vcpkg/installed/x64-osx-dynamic/lib
LIBS = -L$(VCPKG) -lcred `pkg-config --libs openssl libcurl`
OBJS = obj/init.o obj/config.o obj/util.o \
			obj/token_store.o obj/token_refresher.o obj/platform.o \
			obj/untracked.o

# link everything together
plsync bin/plsync: obj/plsync.o $(OBJS)
	clang++ -o bin/plsync $(LIBS) obj/plsync.o $(OBJS)
	# TODO for prod need to make vcpkg install within project
	install_name_tool -change @rpath/libcred.1.dylib $(VCPKG)/libcred.1.dylib bin/plsync
	chmod u+x bin/plsync


# object files
obj/plsync.o: src/plsync.cpp include/init.h
	clang++ -o obj/plsync.o -c $(CXX) src/plsync.cpp

obj/init.o: src/init.cpp include/init.h include/config.h include/token_store.h include/util.h
	clang++ -o obj/init.o -c $(CXX) src/init.cpp

obj/config.o: src/config.cpp include/config.h include/platform.h include/client_secret.h
	clang++ -o obj/config.o -c $(CXX) src/config.cpp

obj/util.o: src/util.cpp include/util.h
	clang++ -o obj/util.o -c $(CXX) src/util.cpp

obj/token_store.o: src/token_store.cpp include/token_store.h
	clang++ -o obj/token_store.o -c $(CXX) src/token_store.cpp

obj/token_refresher.o: src/token_refresher.cpp include/token_refresher.h include/token_store.h
	clang++ -o obj/token_refresher.o -c $(CXX) src/token_refresher.cpp

obj/platform.o: src/platform.cpp include/platform.h
	clang++ -o obj/platform.o -c $(CXX) src/platform.cpp

obj/untracked.o: src/untracked.cpp include/untracked.h
	clang++ -o obj/untracked.o -c $(CXX) src/untracked.cpp
	

# tests
tests bin/test: test/* obj/token_store.o obj/token_refresher.o
	clang++ -o bin/test test/test.cpp $(CXX) $(LIBS) $(OBJS)
	# TODO for prod need to make vcpkg install within project
	install_name_tool -change @rpath/libcred.1.dylib $(VCPKG)/libcred.1.dylib bin/test

all:
	make clean
	make plsync
	make tests

clean:
	rm -rf obj bin
	mkdir obj bin
