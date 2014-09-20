/* See LICENSE for licence details. */

/*
 * LUNA framebuffer support is based on:
 *    OpenBSD:src/sys/arch/luna88k/dev/omrasops.c
 */
/* $OpenBSD: omrasops.c,v 1.11 2014/01/02 15:30:34 aoyama Exp $ */
/* $NetBSD: omrasops.c,v 1.1 2000/01/05 08:48:56 nisimura Exp $ */
/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	ALL1BITS	(~0U)
#define	BLITWIDTH	(32)
#define	ALIGNMASK	(0x1f)
#define	BYTESDONE	(4)
#define	PLANESIZE	0x40000

static inline void draw_sixel(struct framebuffer *fb, int line, int col, uint8_t *pixmap)
{
	int h, w, src_offset;
	uint32_t pixel, color = 0;
	int x, y, i;
	uint32_t *p0, *p;

	for (h = 0; h < CELL_HEIGHT; h++) {
		for (w = 0; w < CELL_WIDTH; w++) {
			src_offset = BYTES_PER_PIXEL * (h * CELL_WIDTH + w);
			memcpy(&color, pixmap + src_offset, BYTES_PER_PIXEL);

			pixel = color2pixel(&fb->vinfo, color);

			x = CELL_WIDTH * col + w;
			y = CELL_HEIGHT * line + h;
			p0 = (uint32_t *)((uint8_t *)fb->buf
				+ y * fb->line_length + ((x / 32) * 4));
			x &= ALIGNMASK;	/* x % 32 */

			for (i = 0; i < DEPTH; i++) {
				p = (uint32_t *)((uint8_t *)p0 + PLANESIZE * i);
				if (pixel & (0x01 << i))
					/* set */
					*p |= (0x00000001 << (31 - x));
				else
					/* reset */
					*p &= ~(0x00000001 << (31 - x));
			}
		}
	}
}

static inline void draw_glyph(uint32_t *p0, int plane, uint32_t glyph,
	int fg, int bg, uint32_t mask)
{
	uint32_t glyphbg, fgpat, bgpat;
	uint32_t *p;

	glyphbg = glyph ^ ALL1BITS;
	fgpat = (fg & (0x01 << plane)) ? glyph : 0;
	bgpat = (bg & (0x01 << plane)) ? glyphbg : 0;
	p = (uint32_t *)((uint8_t *)p0 + PLANESIZE * plane);
	*p = (*p & ~mask) | ((fgpat | bgpat) & mask);
}

static inline void draw_line(struct framebuffer *fb, struct terminal *term, int line)
{
	int pos, size, col, glyph_width, bdf_shift;
	struct color_pair_t color_pair;
	struct cell_t *cellp;

	int x, y, width, height, align, fg, bg, plane;
	u_int32_t lmask, rmask, glyph;
	u_int8_t *p;

	for (col = term->cols - 1; col >= 0; col--) {

		/* target cell */
		cellp = &term->cells[line][col];

		/* draw sixel pixmap */
		if (cellp->has_pixmap) {
			draw_sixel(fb, line, col, cellp->pixmap);
			continue;
		}

		/* copy current color_pair (maybe changed) */
		color_pair = cellp->color_pair;

		/* check wide character or not */
		if (cellp->width == NEXT_TO_WIDE)
			continue;

		glyph_width = (cellp->width == HALF) ? CELL_WIDTH: CELL_WIDTH * 2;
		bdf_shift = 32 - my_ceil(glyph_width, BITS_PER_BYTE) * BITS_PER_BYTE;
		/* check cursor positon */
		if ((term->mode & MODE_CURSOR && line == term->cursor.y)
			&& (col == term->cursor.x
			|| (cellp->width == WIDE && (col + 1) == term->cursor.x)
			|| (cellp->width == NEXT_TO_WIDE && (col - 1) == term->cursor.x))) {
			color_pair.fg = DEFAULT_BG;
			color_pair.bg = (!tty.visible && BACKGROUND_DRAW) ? PASSIVE_CURSOR_COLOR: ACTIVE_CURSOR_COLOR;
		}
		/* color palette */
		fg = term->color_palette[color_pair.fg];
		bg = term->color_palette[color_pair.bg];
#if 0	/* Not yet on LUNA */
		if (fb->wall && color_pair.bg == DEFAULT_BG) /* wallpaper */
			memcpy(&pixel, fb->wall + pos, fb->bytes_per_pixel);
#endif

		x = CELL_WIDTH * col;
		y = CELL_HEIGHT * line;

		p = (u_int8_t *)fb->buf + y * fb->line_length + ((x / 32) * 4);
		align = x & ALIGNMASK;
		width = cellp->glyphp->width * CELL_WIDTH + align;
		lmask = ALL1BITS >> align;
		rmask = ALL1BITS << (-width & ALIGNMASK);
		height = 0;

		if (width <= BLITWIDTH) {
			lmask &= rmask;
			while (height < CELL_HEIGHT) {
				/* if UNDERLINE attribute on, swap bg/fg */
				if ((height == (CELL_HEIGHT - 1))
					&& (cellp->attribute & attr_mask[ATTR_UNDERLINE]))
						bg = fg;
				glyph = (uint32_t)cellp->glyphp->bitmap[height];
				/* shift leftmost */
				glyph <<= bdf_shift;
				glyph = glyph >> align;

				for (plane = 0; plane < DEPTH; plane++)
					draw_glyph((uint32_t *)p, plane, glyph, fg, bg, lmask);

				p += fb->line_length;
				height++;
			}
		} else {
			u_int8_t *q = p;
			u_int32_t lhalf, rhalf;

			while (height < CELL_HEIGHT) {
				/* if UNDERLINE attribute on, swap bg/fg */
				if ((height == (CELL_HEIGHT - 1))
					&& (cellp->attribute & attr_mask[ATTR_UNDERLINE]))
						bg = fg;
				glyph = (uint32_t)cellp->glyphp->bitmap[height];
				/* shift leftmost */
				glyph <<= bdf_shift;
				lhalf = glyph >> align;

				for (plane = 0; plane < DEPTH; plane++)
					draw_glyph((uint32_t *)p, plane, lhalf, fg, bg, lmask);

				p += BYTESDONE;
				rhalf = glyph << (BLITWIDTH - align);

				for (plane = 0; plane < DEPTH; plane++)
					draw_glyph((uint32_t *)p, plane, rhalf, fg, bg, rmask);

				p = (q += fb->line_length);
				height++;
			}
		}
	}

	/* actual display update (bit blit) */
	size = TERM_WIDTH / 8;
	for (height = 0; height < CELL_HEIGHT; height++) {
		for (plane = 0; plane < DEPTH; plane++) {
			pos = (line * CELL_HEIGHT + height) * fb->line_length
				+ PLANESIZE * plane;
			memcpy(fb->fp + pos, fb->buf + pos, size);
		}
	}

	/* TODO: page flip
		if fb_fix_screeninfo.ypanstep > 0, we can use hardware panning.
		set fb_fix_screeninfo.{yres_virtual,yoffset} and call ioctl(FBIOPAN_DISPLAY)
		but drivers  of recent hardware (inteldrmfb, nouveaufb, radeonfb) don't support...
		(maybe we can use this by using libdrm) */
	/* TODO: vertical synchronizing */

	term->line_dirty[line] = ((term->mode & MODE_CURSOR) && term->cursor.y == line) ? true: false;
}

void refresh(struct framebuffer *fb, struct terminal *term)
{
	int line;

	if (term->mode & MODE_CURSOR)
		term->line_dirty[term->cursor.y] = true;

	for (line = 0; line < term->lines; line++) {
		if (term->line_dirty[line])
			draw_line(fb, term, line);
	}
}
