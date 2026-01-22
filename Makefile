cflags = $(CFLAGS) -Wall -Wextra -Wpedantic
ldflags = $(LDFLAGS) -lX11 -lXss

all: xsscmd

install: xsscmd
	install -Dm 755 xsscmd $(PREFIX)/bin/xsscmd

xsscmd: xsscmd.c
	$(CC) -o $@ $(cflags) $< $(ldflags)
