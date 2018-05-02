CXX=g++
CXXFLAGS=-g -O0 -Wall
LDFLAGS=-static
OBJS=main

all: $(OBJS)

clean:
	$(RM) $(OBJS)

