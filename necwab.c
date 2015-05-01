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
#include <unistd.h>		/* close(2) */
#include <sys/mman.h>		/* mmap(2) */
#include <sys/types.h>

#include "nec_cirrus.h"
#include "necwab.h"

int wab_iofd, wab_memfd;
u_int8_t *pc98iobase, *pc98membase;
u_int8_t *wab_iobase,*wab_membase;

int
necwab_init(int mode)
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

	board_type = necwab_ident_board();
	if (board_type != 0x60) {
		printf("No WAB found\n");
		goto exit2;
	}

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

	nec_cirrus_init(mode);
	return 0;

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
	nec_cirrus_leave();
	munmap((void *)pc98membase, 0x1000000);
	close(wab_memfd);
	munmap((void *)pc98iobase, 0x10000);
	close(wab_iofd);
}

inline void
necwab_outb(u_int16_t index, u_int8_t data)
{
	*(pc98iobase + index) = data;
}

inline u_int8_t
necwab_inb(u_int16_t index)
{
	return *(pc98iobase + index);
}

int
necwab_ident_board(void)
{
	u_int8_t data;

	necwab_outb(NECWAB_INDEX, 0x00);
	data = necwab_inb(NECWAB_DATA);

	switch (data) {
	case 0x60:
		break;
	default:
		printf("No supported WAB found, ID = 0x%02x\n", data);
		data = -1;
		break;
	}

	return (int)data;
}
