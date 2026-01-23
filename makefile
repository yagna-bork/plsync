CXX = -std=c++11
VCPKG = $(HOME)/Applications/vcpkg/installed/x64-osx-dynamic/lib
LIBS = -L$(VCPKG) -lcred `pkg-config --libs openssl libcurl`

# link everything together
plsync bin/plsync: obj/plsync.o obj/init.o obj/config.o obj/httplib.o
	clang++ -o bin/plsync $(LIBS) obj/plsync.o obj/init.o obj/config.o obj/httplib.o
	# for prod need to make vcpkg install within project
	install_name_tool -change @rpath/libcred.1.dylib $(VCPKG)/libcred.1.dylib bin/plsync
	chmod u+x bin/plsync

# object files
obj/plsync.o: src/plsync.cpp include/init.h
	clang++ -o obj/plsync.o -c $(CXX) src/plsync.cpp

obj/init.o: src/init.cpp include/init.h include/config.h include/httplib.h
	clang++ -o obj/init.o -c $(CXX) src/init.cpp

obj/config.o: src/config.cpp include/config.h
	clang++ -o obj/config.o -c $(CXX) src/config.cpp

obj/httplib.o: src/httplib.cpp include/httplib.h
	clang++ -o obj/httplib.o -c $(CXX) src/httplib.cpp

clean:
	rm -rf obj bin
	mkdir obj bin
	
