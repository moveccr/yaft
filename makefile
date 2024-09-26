CC ?= gcc
#CC ?= clang

#CFLAGS  ?= -std=c99 -pedantic -Wall -Wextra -O3 -s -pipe
#CFLAGS  = -std=c99 -pedantic -Wall -Wextra -O3 -s -pipe
CFLAGS  = -std=c99 -O2 -pipe
#CFLAGS  = -std=c99 -g -O0 -pipe
LDFLAGS ?=

XCFLAGS  ?= -std=c99 -pedantic -Wall -Wextra -I/usr/include/X11/ -O3 -s -pipe
XLDFLAGS ?= -lX11

HDR = color.h conf.h dcs.h draw.h function.h osc.h parse.h terminal.h util.h yaft.h glyph.h \
	fb/linux.h fb/freebsd.h fb/netbsd.h fb/openbsd.h x/x.h

DESTDIR   = /usr/local
PREFIX    = $(DESTDIR)
MANPREFIX = $(DESTDIR)/man

all: yaft

yaft: mkfont_bdf

yaftx: mkfont_bdf

mkfont_bdf: tools/mkfont_bdf.c tools/font.h tools/bdf.h
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

glyph.c: mkfont_bdf
	# If you want to use your favorite fonts, please change following line
	# usage: mkfont_bdf ALIAS BDF1 BDF2 BDF3... > glyph.h
	# ALIAS: glyph substitution rule file (see table/alias for more detail)
	# BDF1 BDF2 BDF3...: monospace bdf files (must be the same size)
	./mkfont_bdf table/alias fonts/KH-Dot-Hibiya-24.bdf
#./mkfont_bdf table/alias fonts/milkjf_8x16.bdf fonts/milkjf_8x16r.bdf fonts/milkjf_k16.bdf

glyph.o: glyph.c glyph.h
	$(CC) -o $@ -c $< $(CFLAGS) $(LDFLAGS)

yaft: yaft.c $(HDR) glyph.o
	# If you want to change configuration, please modify conf.h before make (see conf.h for more detail)
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) glyph.o

yaftx: x/yaftx.c $(HDR)
	# If you want to change configuration, please modify conf.h before make (see conf.h for more detail)
	$(CC) -o $@ $< $(XCFLAGS) $(XLDFLAGS)

install:
	mkdir -p $(PREFIX)/share/terminfo
	tic -o $(PREFIX)/share/terminfo info/yaft.src
	mkdir -p $(PREFIX)/bin/
	install -m755 ./yaft $(PREFIX)/bin/yaft
	install -m755 ./yaft_wall $(PREFIX)/bin/yaft_wall
	mkdir -p $(MANPREFIX)/man1/
	install -m644 ./man/yaft.1 $(MANPREFIX)/man1/yaft.1

uninstall:
	rm -f $(PREFIX)/bin/yaft
	rm -f $(PREFIX)/bin/yaft_wall

allclean:
	rm -f yaft yaftx mkfont_bdf glyph.h glyph.c *.o

clean:
	rm -f yaft yaftx
