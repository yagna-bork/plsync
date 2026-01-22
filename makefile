VCPKG = $(HOME)/Applications/vcpkg/installed/x64-osx
LIBS = -L$(VCPKG)/lib -lcred `pkg-config --libs openssl libcurl`
CXX = -std=c++11

# link everything together
plsync bin/plsync: obj/plsync.o obj/init.o obj/config.o obj/httplib.o
	g++ -o bin/plsync $(LIBS) obj/plsync.o obj/init.o obj/config.o obj/httplib.o
	chmod u+x bin/plsync

# object files
obj/plsync.o: src/plsync.cpp include/init.h
	g++ -o obj/plsync.o -c $(CXX) src/plsync.cpp

obj/init.o: src/init.cpp include/init.h include/config.h include/httplib.h
	g++ -o obj/init.o -c $(CXX) src/init.cpp

obj/config.o: src/config.cpp include/config.h
	g++ -o obj/config.o -c $(CXX) src/config.cpp

obj/httplib.o: src/httplib.cpp include/httplib.h
	g++ -o obj/httplib.o -c $(CXX) src/httplib.cpp
