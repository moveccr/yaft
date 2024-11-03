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

#define	PLANESIZE	0x40000

static struct framebuffer *gFB;
static uint32_t draw_scroll_y;

#if CELL_WIDTH > 32
 #error "CELL_WIDTH > 32"
#endif

static inline uint32_t
ROL(uint32_t v, uint32_t n)
{
	return (v << n) | (v >> (32 - n));
}

static inline uint32_t
ROR(uint32_t v, uint32_t n)
{
	return (v >> n) | (v << (32 - n));
}

static inline uint8_t*
next_raster_ptr(struct framebuffer *fb, void *p)
{
	uint32_t a;
	a = (uint32_t)p - (uint32_t)(fb->plane);
	a += 0x100;
	a &= 0x3FFFF;
	a += (uint32_t)(fb->plane);
//DPRINTF("p=%08x a=%08x\n", (uint32_t)p, a);
	return (uint8_t *)a;
}

static inline void
draw_pix(struct framebuffer *fb, int line, int col, uint8_t *pixmap)
{
	uint32_t h, w;
	uint8_t color = 0;
	uint32_t x;
	uint8_t *pp = pixmap;
	uint32_t m, m0;
	uint32_t *dst, *dstY;

//DPRINTF("draw_pix(*,line=%d,col=%d,pixmap=%08x)\n", line, col, pixmap);

	x = CELL_WIDTH * col;
	m0 = 0x80000000U >> (x % 32);
	dstY = fb_ptr(fb, x, CELL_HEIGHT * line + draw_scroll_y);
//DPRINTF("dstY=%08x\n", dstY);

	for (h = 0; h < CELL_HEIGHT; h++) {
		m = m0;
		dst = dstY;

		for (w = 0; w < CELL_WIDTH; w++) {
			color = *pp++;

			setBMSEL(fb, 0xff);
			setROPc(fb, ROP_ZERO, m);
			setBMSEL(fb, color);
			setROPc(fb, ROP_ONE, m);
			setBMSEL(fb, 0xff);
			*(volatile uint32_t *)dst = 0;	/* any data write */

			m >>= 1;
			if (m == 0) {
				m = 1U << 31;
				dst++;
			}
		}
		dstY = (uint32_t *)next_raster_ptr(fb, dstY);
	}
	//DPRINTF("exit draw_pix\n");
}

static inline void
draw_char(struct framebuffer *fb,
	uint32_t line, uint32_t col,
	int fg, int bg, const glyphbitmap_t *bmp, int ch_width, int bdf_shift,
	int underline)
{
	uint32_t x, xL;
	uint32_t h;
	uint32_t *dst, *dst0;
	const glyphbitmap_t *src;
	uint32_t m;
	uint32_t m0;

//DPRINTF("draw_char(*,line=%d,col=%d,fg=$%x,bg=$%x,bmp=$%08x,chw=%d,shift=%d,ul=%d)\n", line, col, fg, bg, bmp, ch_width, bdf_shift, underline);

	m0 = ((1U << ch_width) - 1) << (32 - ch_width);

	x = CELL_WIDTH * col;
	dst0 = fb_ptr(fb, x, CELL_HEIGHT * line + draw_scroll_y);
// DPRINTF("dst0=%08x\n", dst0);

	xL = x % 32;
	m = m0 >> xL;
	dst = dst0;

	do {
		src = bmp;

		/* see also NetBSD omrasops.c omfb_drawchar() */
		setBMSEL(fb, fg & bg);
		setROPc(fb, ROP_ONE, m);
		setBMSEL(fb, fg & ~bg);
		setROPc(fb, ROP_THROUGH, m);
		setBMSEL(fb, ~fg & bg);
		setROPc(fb, ROP_INV1, m);
		setBMSEL(fb, ~fg & ~bg);
		setROPc(fb, ROP_ZERO, m);
		setBMSEL(fb, 0xff);

		for (h = 0; h < CELL_HEIGHT - 1; h++) {
			uint32_t b = *src++;
			b <<= bdf_shift;
			b >>= xL;

// DPRINTF("dst=%x\n", dst);
			*(volatile uint32_t *)dst = b;
			dst = (uint32_t *)next_raster_ptr(fb, dst);
		}
		/* if UNDERLINE on, invert at bottom line */
		{
			uint32_t b = *src++;
			b <<= bdf_shift;
			b >>= xL;
			if (underline) {
				b = ~b;
			}

// DPRINTF("dst=%x\n", dst);
			*(volatile uint32_t *)dst = b;
		}

		if (xL + ch_width <= 32) {
			// DPRINTF("exit draw_char()\n");
			return;
		}

		xL = xL + ch_width - 32;
		bdf_shift += ch_width - xL;
		m = m0 & ~(m0 >> xL);

		xL = 0;
		dst = dst0 + 1;
//DPRINTF("m=%08x dst=%08x\n", m, dst);
	} while (1);
}

uint8_t draw_modmap_0[1280/8*1024/16] = {0};
#define DRAW_MODMAP(x, y) draw_modmap_0[(x) + (y) * 160]
uint8_t draw_prev_blank_bg = 0;
#define DRAW_MODMAP_CHAR 0
#define DRAW_MODMAP_BLANK 1

static inline void
draw_line(struct framebuffer *fb, struct terminal *term, int line)
{
	int col, glyph_width, bdf_shift;
	struct color_pair_t color_pair;
	struct cell_t *cellp;

	int fg, bg;
	bool is_cursorline;

//DPRINTF("draw_line(*,*,%d)\n", line);
	
	is_cursorline = (term->mode & MODE_CURSOR && line == term->cursor.y);

	for (col = term->cols - 1; col >= 0; col--) {

		/* target cell */
		cellp = &term->cells[line][col];

		/* draw sixel pixmap */
		if (cellp->has_pixmap) {
			draw_pix(fb, line, col, cellp->pixmap);
			DRAW_MODMAP(col, line) = DRAW_MODMAP_CHAR;
			continue;
		}

		/* copy current color_pair (maybe changed) */
		color_pair = cellp->color_pair;

		/* check wide character or not */
		if (cellp->width == NEXT_TO_WIDE) {
			DRAW_MODMAP(col, line) = DRAW_MODMAP_CHAR;
			continue;
		} else if (cellp->width == HALF) {
			glyph_width = CELL_WIDTH;
		} else { /* WIDE */
			glyph_width = CELL_WIDTH * 2;
		}


		/* check cursor positon */
		if (is_cursorline
			&& (col == term->cursor.x
			|| (cellp->width == WIDE && (col + 1) == term->cursor.x)
			|| (cellp->width == NEXT_TO_WIDE && (col - 1) == term->cursor.x))) {
			color_pair.fg = DEFAULT_BG;
			color_pair.bg = (!tty.visible && BACKGROUND_DRAW) ? PASSIVE_CURSOR_COLOR: ACTIVE_CURSOR_COLOR;
			DRAW_MODMAP(col, line) = DRAW_MODMAP_CHAR;

		} else if (cellp->glyphp == term->glyph_map[DEFAULT_CHAR]) {
			/* blank skip */
			if (draw_prev_blank_bg == color_pair.bg
			 && DRAW_MODMAP(col, line) == DRAW_MODMAP_BLANK) {
				// XXX MODMAP はもう不要?
				//continue;
			}
			DRAW_MODMAP(col, line) = DRAW_MODMAP_BLANK;
			// XXX 文字単位 dirty を上位で頑張るべき
			draw_prev_blank_bg = color_pair.bg;
		} else {
			DRAW_MODMAP(col, line) = DRAW_MODMAP_CHAR;
		}

		/* color palette */
		fg = term->color_palette[color_pair.fg];
		bg = term->color_palette[color_pair.bg];

#if 0	/* Not yet on LUNA */
		if (fb->wall && color_pair.bg == DEFAULT_BG) /* wallpaper */
			memcpy(&pixel, fb->wall + pos, fb->bytes_per_pixel);
#endif

		bdf_shift = 32 - sizeof(glyphbitmap_t) * 8;
		draw_char(fb, line, col,
			fg, bg,
			cellp->glyphp->bitmap,
			glyph_width, bdf_shift,
			(int)(cellp->attribute & attr_mask[ATTR_UNDERLINE]));
	}

	/* TODO: page flip
		if fb_fix_screeninfo.ypanstep > 0, we can use hardware panning.
		set fb_fix_screeninfo.{yres_virtual,yoffset} and call ioctl(FBIOPAN_DISPLAY)
		but drivers  of recent hardware (inteldrmfb, nouveaufb, radeonfb) don't support...
		(maybe we can use this by using libdrm) */
	/* TODO: vertical synchronizing */

	term->rows[line].drawreq = is_cursorline ?  DRAWREQ_YES : DRAWREQ_NO;
}


void refresh(struct framebuffer *fb, struct terminal *term)
{
	int line;

	gFB = fb;

//DPRINTF("refresh\n");
	if (term->mode & MODE_CURSOR) {
		term->rows[term->cursor.y].drawreq = DRAWREQ_YES;
	}
//DPRINTF("refresh B\n");
	for (line = 0; line < term->lines; line++) {
		// ここでは SCROLL も描画対象
//DPRINTF("[%d].drawreq=%d\n", line, term->rows[line].drawreq);
		if (term->rows[line].drawreq != DRAWREQ_NO) {
//DPRINTF("refresh C\n");
			draw_line(fb, term, line);
		}
	}
//DPRINTF("refresh D\n");
}

void
draw_scroll(struct terminal *term, int from, int to, int offset)
{
	struct framebuffer *fb = gFB;
	int i;
//DPRINTF("draw_scroll(*,from=%d,to=%d,offset=%d)\n", from, to, offset);

	if (fb == NULL) {
//DPRINTF("fb=NULL\n");
		return;
	}
	if (from != 0) {
		return;
	}
	if (offset <= 0) {
		return;
	}

	setBMSEL(fb, 0xff);
	setROPc(fb, ROP_ZERO, 0xffffffffU);
	uint8_t *p = (uint8_t *)fb_ptr(fb, 0, draw_scroll_y);
	for (i = 0; i < offset * CELL_HEIGHT; i++) {
		memset(p, 0, fb->width / 8);
		p = next_raster_ptr(fb, p);
	}

	draw_scroll_y += offset * CELL_HEIGHT;
	draw_scroll_y &= 0x3ff;

	for (i = 0; i < term->lines; i++) {
		if (term->rows[i].drawreq == DRAWREQ_SCROLL) {
			term->rows[i].drawreq = DRAWREQ_NO;
		}
	}

	setRFCNT(fb, draw_scroll_y);
//DPRINTF("draw_scroll_y=%u\n", draw_scroll_y);
}
