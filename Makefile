ARCH := $(shell getconf LONG_BIT)
CPPFLAGS := -I/usr/include/libxml2 -DFMT_HEADER_ONLY=1 -DFMT_STRING_ALIAS=1
CXXFLAGS := -std=c++14 -fstack-protector-all -g3 -O3 -Wall -Wextra -Werror -Wno-unused-result

all: smatool

smatool: smatool.o repost.o sma_mysql.o almanac.o sb_commands.o
	$(CXX) $^ -lxml2 -lmysqlclient -lbluetooth -lcurl -lm -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

smatool.o: smatool.cpp sma_mysql.hpp
repost.o: repost.cpp sma_mysql.hpp
sma_mysql.o: sma_mysql.cpp
almanac.o: almanac.cpp
sma_pvoutput.o: sma_pvoutput.cpp
sb_commands.o: sb_commands.cpp

clean:
	rm -f *.o
install:
	install -m 755 smatool /usr/local/bin
	install -m 644 sma.in.new /usr/local/bin
	install -m 644 smatool.conf.new /usr/local/etc
	install -m 644 smatool.xml /usr/local/bin

.PHONY: all clean install
