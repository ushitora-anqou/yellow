CXX=g++
CXXFLAGS=-g -O0 -Wall -std=c++17
LDFLAGS=-static
OBJS=main

all: $(OBJS)

clean:
	$(RM) $(OBJS)

