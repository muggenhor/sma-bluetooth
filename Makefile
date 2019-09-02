ARCH := $(shell getconf LONG_BIT)
CPPFLAGS := -DFMT_HEADER_ONLY=1 -DFMT_STRING_ALIAS=1
CXXFLAGS := -std=c++14 -fstack-protector-all -g3 -O3 -Wall -Wextra -Werror -Wno-unused-result

all: smatool

smatool: smatool.o sb_commands.o
	$(CXX) $^ -lbluetooth -lm -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

smatool.o: smatool.cpp sma_map.ipp
sb_commands.o: sb_commands.cpp

clean:
	rm -f *.o
install:
	install -m 755 smatool /usr/local/bin
	install -m 644 sma.in.new /usr/local/bin
	install -m 644 smatool.conf.new /usr/local/etc

.PHONY: all clean install
