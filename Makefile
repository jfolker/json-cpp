# Works on pmake and gmake, boilerplate Makefile for messing around in C++
CXX=clang++
CXXFLAGS=-std=c++17 -lpthread -O0 -ggdb -Wall -Werror

build:
	$(CXX) $(CXXFLAGS) -o main *.cpp

clean:
	rm -f main *.o *.i *.core

