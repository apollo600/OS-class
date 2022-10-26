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