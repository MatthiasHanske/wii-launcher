CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../launcher/source
SRCS     := $(wildcard *.cpp) $(wildcard ../launcher/source/core/*.cpp)
OBJS     := $(SRCS:.cpp=.o)

.PHONY: test clean

test: run_tests
	./run_tests

run_tests: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f run_tests $(OBJS)
