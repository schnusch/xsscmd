cflags = $(CFLAGS) -Wall -Wextra -Wpedantic
ldflags = $(LDFLAGS) -lX11 -lXss

all: xsscmd

install: xsscmd
	install -Dm 755 xsscmd $(DESTDIR)$(PREFIX)/bin/xsscmd

xsscmd: xsscmd.c
	$(CC) -o $@ $(cflags) $< $(ldflags)
