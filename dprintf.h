
#ifndef _DPRINTF_H_
#define _DPRINTF_H_

#if defined(USE_DPRINTF)

#include <stdarg.h>
#include <stdio.h>

static void
vDprintf(const char *fmt, va_list ap)
{
	FILE *fp;
	fp = fopen("yaft.debuglog", "a+");
	vfprintf(fp, fmt, ap);
	fclose(fp);
}

static void
Dprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vDprintf(fmt, ap);
	va_end(ap);
}

#define DPRINTF(...)	Dprintf(__VA_ARGS__)
#else
#define DPRINTF(...)	((void)0)
#endif

#endif	/* _DPRINTF_H_ */
