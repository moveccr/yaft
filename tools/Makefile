
CC ?= gcc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -O3 -s -pipe
LDFLAGS ?=
XCFLAGS  ?= -std=c99 -pedantic -Wall -Wextra -I/usr/include/X11/ -O3 -s -pipe
XLDFLAGS ?= -lX11

mkfont_bdf: mkfont_bdf.c font.h bdf.h
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)
