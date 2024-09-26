void fatal(char *str)
{
	perror(str);
	exit(EXIT_FAILURE);
}

FILE *efopen(char *path, char *mode)
{
	FILE *fp;
	errno = 0;

	if ((fp = fopen(path, mode)) == NULL) {
		fprintf(stderr, "cannot open \"%s\"\n", path);
		fatal("fopen");
	}
	return fp;
}

void efclose(FILE *fp)
{
	errno = 0;

	if (fclose(fp) < 0)
		fatal("fclose");
}

void *ecalloc(size_t nmemb, size_t size)
{
	void *ptr;
	errno = 0;

	if ((ptr = calloc(nmemb, size)) == NULL)
		fatal("calloc");

	return ptr;
}

/* for yaft original font format: not used

CODE
WIDTH HEIGHT
BITMAP
BITMAP
BITMAP
CODE
WIDTH HEIGHT
....

void load_glyph(struct glyph_t *fonts, char *path)
{
	int count = 0, state = 0;
	char buf[BUFSIZE], *endp;
	FILE *fp;
	uint16_t code = DEFAULT_CHAR;

	fp = efopen(path, "r");

	while (fgets(buf, BUFSIZE, fp) != NULL) {
		if (strlen(buf) == 0 || buf[0] == '#')
			continue;

		switch (state) {
		case 0:
			code = atoi(buf);
			if (fonts[code].bitmap != NULL) {
				free(fonts[code].bitmap);
				fonts[code].bitmap = NULL;
			}
			state = 1;
			break;
		case 1:
			sscanf(buf, "%hhu %hhu", &fonts[code].width, &fonts[code].height);
			fonts[code].bitmap = (uint32_t *) emalloc(fonts[code].height * sizeof(uint32_t));
			state = 2;
			break;
		case 2:
			fonts[code].bitmap[count++] = strtoul(buf, &endp, 16);
			if (count >= fonts[code].height)
				state = count = 0;
			break;
		default:
			break;
		}
	}

	efclose(fp);
}
*/

void load_alias(struct glyph_t *fonts, char *alias)
{
	unsigned int dst, src;
	char buf[BUFSIZE];
	FILE *fp;

	fp = efopen(alias, "r");

	while (fgets(buf, BUFSIZE, fp) != NULL) {
		if (strlen(buf) == 0 || buf[0] == '#')
			continue;

		sscanf(buf, "%X %X", &dst, &src);
		if ((dst >= UCS2_CHARS) || (src >= UCS2_CHARS))
			continue;

		if (fonts[src].bitmap != NULL) {
			fprintf(stderr, "swapped: use U+%.4X for U+%.4X\n", src, dst);

			free(fonts[dst].bitmap);
			fonts[dst].width = fonts[src].width;
			fonts[dst].height = fonts[src].height;
			fonts[dst].bitmap = fonts[src].bitmap;
		}
	}

	efclose(fp);
}

void set_empty_glyph(struct glyph_t *fonts, uint32_t code, enum glyph_width_t wide)
{
	fonts[code].width  = fonts[DEFAULT_CHAR].width * wide;
	fonts[code].height = fonts[DEFAULT_CHAR].height;
	fonts[code].bitmap = (uint32_t *) ecalloc(fonts[DEFAULT_CHAR].height, sizeof(uint32_t));
}

void check_fonts(struct glyph_t *fonts)
{
	if (fonts[DEFAULT_CHAR].bitmap == NULL) {
		fprintf(stderr, "default glyph(U+%.4X) not found\n", DEFAULT_CHAR);
		exit(EXIT_FAILURE);
	}

	if (fonts[SUBSTITUTE_HALF].bitmap == NULL) {
		fprintf(stderr, "half substitute glyph(U+%.4X) not found, use empty glyph\n", SUBSTITUTE_HALF);
		set_empty_glyph(fonts, SUBSTITUTE_HALF, HALF);
	}

	if (fonts[SUBSTITUTE_WIDE].bitmap == NULL) {
		fprintf(stderr, "wide substitute glyph(U+%.4X) not found, use empty glyph\n", SUBSTITUTE_WIDE);
		set_empty_glyph(fonts, SUBSTITUTE_WIDE, WIDE);
	}

	if (fonts[REPLACEMENT_CHAR].bitmap == NULL) {
		fprintf(stderr, "replacement glyph(U+%.4X) not found, use empty glyph\n", REPLACEMENT_CHAR);
		set_empty_glyph(fonts, REPLACEMENT_CHAR, HALF);
	}
}

void dump_fonts(struct glyph_t *fonts)
{
	int i, j, width;
	uint8_t cell_width, cell_height;
	int glyphs_count = 0;
	FILE *fp_head;
	FILE *fp_body;

	cell_width = fonts[DEFAULT_CHAR].width;
	cell_height = fonts[DEFAULT_CHAR].height;

	fp_head = efopen("glyph.h", "w");
	fp_body = efopen("glyph.c", "w");

	fprintf(fp_head, "#ifndef _GLYPH_H_\n");
	fprintf(fp_head, "#define _GLYPH_H_\n");
	fprintf(fp_head, "#include <stdint.h>\n");
	fprintf(fp_head, "typedef uint%d_t glyphbitmap_t;\n",
	  ((cell_width + BITS_PER_BYTE - 1) / BITS_PER_BYTE) * BITS_PER_BYTE * 2);
		
	fprintf(fp_head,
		"struct glyph_t {\n"
		"\tuint32_t code;\n"
		"\tuint8_t width;\n"
		"\tglyphbitmap_t bitmap[%d];\n"
		"};\n\n", cell_height);

	fprintf(fp_head, "#define CELL_WIDTH (%d)\n", cell_width);
	fprintf(fp_head, "#define CELL_HEIGHT (%d)\n", cell_height);


	fprintf(fp_body, "#include \"glyph.h\"\n");
	fprintf(fp_body, "const struct glyph_t glyphs[] = {\n");
	for (i = 0; i < UCS2_CHARS; i++) {
		width = wcwidth(i);

		if (width <= 0) {
			/* not printable */
			fprintf(fp_body, "/* %d width %d <= 0 */\n", i, width);
		} else if (fonts[i].bitmap == NULL) {
			/* glyph not found */
			fprintf(fp_body, "/* %d width %d, bitmap=NULL */\n", i, width);
		} else if (fonts[i].height != cell_height) {
			/* invalid font height */
			fprintf(fp_body, "/* %d width %d, height %d (BAD H) */\n", i, width, fonts[i].height);
		} else if (fonts[i].width != cell_width * width) {
			/* invalid font width */
			fprintf(fp_body, "/* %d width %d, height %d (BAD W) */\n", i, fonts[i].width, fonts[i].height);
		}  else {

			fprintf(fp_body, "\t{%d, %d, {", i, width);
			for (j = 0; j < cell_height; j++)
				fprintf(fp_body, "0x%X%s", fonts[i].bitmap[j], (j == (cell_height - 1)) ? "": ", ");
			fprintf(fp_body, "}},\n");
			glyphs_count++;
		}
	}
	fprintf(fp_body, "};\n");

	fprintf(fp_head, "extern const struct glyph_t glyphs[%d];\n", glyphs_count);
	fprintf(fp_head, "#endif /* _GLYPH_H_ */\n");

	efclose(fp_head);
	efclose(fp_body);
}
