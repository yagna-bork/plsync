VCPKG = $(HOME)/Applications/vcpkg/installed/x64-osx
LIBS = -L$(VCPKG)/lib -lcred `pkg-config --libs openssl`
CXX = -std=c++11

plsync bin/plsync: obj/plsync.o obj/init.o
	g++ -o bin/plsync $(LIBS) obj/plsync.o obj/init.o
	chmod u+x bin/plsync

obj/plsync.o: src/plsync.cpp include/init.h
	g++ -o obj/plsync.o -c $(CXX) src/plsync.cpp

obj/init.o: src/init.cpp include/init.h
	g++ -o obj/init.o -c $(CXX) src/init.cpp
