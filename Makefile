CXX=g++
CXXFLAGS=-g -O0 -Wall -std=c++17 -I./vendor/twitcurl/include -I./vendor/picojson
LDFLAGS=-L./vendor/twitcurl/lib -ltwitcurl
OBJS=main

all: $(OBJS)

clean:
	$(RM) $(OBJS)

