#include <time.h>

static inline uint64_t
getStopwatch()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

#if 0
static char * 
strStopwatch(uint64_t t)
{
	static char str[32];

	uint32_t ns = (uint32_t)(t % 1000);
	t /= 1000;
	uint32_t us = (uint32_t)(t % 1000);
	t /= 1000;

	snprintf(str, sizeof(str), "%llu.%03u'%03u msec",
		t, us, ns);
	return str;
}
#endif
