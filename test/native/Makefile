CXX    ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -Iunity
SRC      = test_main.cpp unity/unity.c

.PHONY: all clean

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o test_native && ./test_native

clean:
	rm -f test_native
