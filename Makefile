ARCH := $(shell getconf LONG_BIT)
CPPFLAGS := -I/usr/include/libxml2
CFLAGS := -fstack-protector-all -g3 -O3 -Wall -Wextra -Werror -Wno-unused-result

all: smatool

smatool: smatool.o repost.o sma_mysql.o almanac.o sb_commands.o
	gcc $^ -lxml2 -lmysqlclient -lbluetooth -lcurl -lm -o $@ 

%.o: %.c
	gcc $(CPPFLAGS) $(CFLAGS) -c $< -o $@

smatool.o: smatool.c sma_mysql.h
repost.o: repost.c sma_mysql.h
sma_mysql.o: sma_mysql.c
almanac.o: almanac.c
sma_pvoutput.o: sma_pvoutput.c
sb_commands.o: sb_commands.c

clean:
	rm -f *.o
install:
	install -m 755 smatool /usr/local/bin
	install -m 644 sma.in.new /usr/local/bin
	install -m 644 smatool.conf.new /usr/local/etc
	install -m 644 smatool.xml /usr/local/bin

.PHONY: all clean install
