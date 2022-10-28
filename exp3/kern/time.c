#include "type.h"
#include "time.h"

size_t timecounter;

/*
 * 时间戳加一
 */
void
timecounter_inc()
{
	timecounter++;
}

/*
 * 获取内核当前的时间戳
 */
size_t
clock()
{
	return timecounter;
}

/*
 * 延迟固定时间
 */
void
milli_delay(int milli_sec)
{
	int t = clock();

	while (((clock() - t) * 1000 / HZ) < milli_sec) {}
}