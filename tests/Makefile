CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../launcher/source -MMD -MP
SRCS     := $(wildcard *.cpp) $(wildcard ../launcher/source/core/*.cpp)
# Objekte unter tests/obj/, nicht neben den Quellen: sonst findet der Wii-Build
# (VPATH über source/core) Host-Objekte und versucht sie zu linken.
OBJDIR   := obj
OBJS     := $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))

vpath %.cpp . ../launcher/source/core

.PHONY: test clean

test: run_tests
	./run_tests

run_tests: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $@

-include $(OBJS:.o=.d)

clean:
	rm -rf run_tests $(OBJDIR) *.o *.d ../launcher/source/core/*.o ../launcher/source/core/*.d
