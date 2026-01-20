VCPKG=$(HOME)/Applications/vcpkg/installed/x64-osx
LIBS=-L$(VCPKG)/lib -lcred

plsync bin/plsync: src/plsync.cpp
	g++ -o bin/plsync -std=c++11 $(LIBS) src/plsync.cpp
	chmod u+x bin/plsync
