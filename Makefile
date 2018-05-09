CXXFLAGS=-g -O0 -MMD -MP -Wall -std=c++17 -I./vendor/liboauthcpp/include -I./vendor/picojson
LDFLAGS=-lcurl
CPPS=main.cpp $(wildcard vendor/liboauthcpp/src/*.cpp)
OBJS=$(CPPS:.cpp=.o)
DEPS=$(OBJS.o=.d)

yellow: $(OBJS)
	g++ -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(OBJS) $(DEPS)

.cpp.o:
	g++ $(CXXFLAGS) -o $@ -c $<

-include $(DEPS)

