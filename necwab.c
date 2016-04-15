/*
 * Copyright (c) 2015 Kenji Aoyama.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 *
 */

#include <stdio.h>
#include <fcntl.h>		/* open(2) */
#include <unistd.h>		/* usleep(3) */
#include <sys/ioctl.h>		/* ioctl(2) */
#include <sys/mman.h>		/* mmap(2) */
#include <sys/types.h>

#include "necwab.h"
#include "nec_cirrus.h"

int wab_iofd, wab_memfd;
u_int8_t *pc98iobase, *pc98membase;
u_int8_t *wab_iobase,*wab_membase;

int
necwab_init(struct board_type_t *bt, int mode)
{
	int board_type;

	wab_iofd = open("/dev/pcexio", O_RDWR, 0600);
	if (wab_iofd == -1) {
		perror("open");
		goto exit1;
	}

	pc98iobase = (u_int8_t *)mmap(NULL, 0x10000, PROT_READ | PROT_WRITE,
	    MAP_SHARED, wab_iofd, 0);
	if (pc98iobase == MAP_FAILED) {
		perror("mmap iobase");
		goto exit2;
	}
	wab_iobase = pc98iobase + 0x0000;

	board_type = necwab_ident_board(bt);
	if (board_type == -1)
		goto exit1;

	wab_memfd = open("/dev/pcexmem", O_RDWR, 0600);
	if (wab_memfd == -1) {
		perror("open mem");
		goto exit2;
	}

	pc98membase = (u_int8_t *)mmap(NULL, 0x1000000, PROT_READ | PROT_WRITE,
	    MAP_SHARED, wab_memfd, 0);
	if (pc98membase == MAP_FAILED) {
		perror("mmap membase");
		goto exit3;
	}
	wab_membase = pc98membase + 0x0000;

	nec_cirrus_init(bt, mode);
	return board_type;

exit3:
	close(wab_memfd);
exit2:
	close(wab_iofd);
exit1:
	return -1;
}

void
necwab_fini(void)
{
	nec_cirrus_fini();
	munmap((void *)pc98membase, 0x1000000);
	close(wab_memfd);
	munmap((void *)pc98iobase, 0x10000);
	close(wab_iofd);
}

u_int8_t
necwab_inb(u_int16_t index)
{
	return *(pc98iobase + index);
}

inline void
necwab_outb(u_int16_t index, u_int8_t data)
{
	*(pc98iobase + index) = data;
}

int
necwab_ident_board(struct board_type_t *bt)
{
	u_int8_t data;
	u_int i;

	bt->offset = 0;

	/* first, try to check 3rd-party-board */
	for (i = 0; i < 0x0f; i += 2) {
		if (i == 8) continue;

		data = necwab_inb(0x51e1 + i);
		if (data == 0xc2) {
			/* MELCO WGN-A/WSN-A found, with offset i */
			bt->type = data;
			bt->offset = i;

			/*
			 * from:
			 * http://s-sasaji.ddo.jp/pcunix/wsn_pcm228r.diff
			 */

			/* WSN mode */
			data = necwab_inb(0x56e1 + bt->offset);
			data &= 0xae;
			data |= 0x51;
			necwab_outb(0x56e1 + bt->offset, data);

			/* WSN SYS ENABLE */
			data = necwab_inb(0x57e1 + bt->offset);
			data &= 0xdf;	/* XXX? */
			data |= 0x50;
			necwab_outb(0x57e1 + bt->offset, data);

#if 0
			/* WSN PCM ENABLE */
			data = necwab_inb(0x5be1 + bt->offset);
			data &= 0xf9;
			data |= 0x02;
			necwab_outb(0x5be1 + bt->offset, data);

			/* WSN FM INT ENABLE */
			data = necwab_inb(0x51e0 + bt->offset);
			data &= 0xfd;
			necwab_outb(0x51e0 + bt->offset, data);
#endif
			return bt->type;
		}
	}

	necwab_outb(NECWAB_INDEX, 0x00);
	data = necwab_inb(NECWAB_DATA);

	switch (data) {
	case 0x20:
		/* PC-9801-85(WAB-B) */
		bt->type = data;
		break;
	case 0x60:
		/* PC-9801-96(WAB-B3) */
		bt->type = data;
		break;
	default:
		printf("No supported WAB found, ID = 0x%02x\n", data);
		bt->type = -1;
		break;
	}

	return bt->type;
}

