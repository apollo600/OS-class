#ifndef MINIOS_TIME_H
#define MINIOS_TIME_H

// 8253 PIT
#define TIMER0		0x40
#define TIMER_MODE 	0x43
#define RATE_GENERATOR	0x34	/* 00-11-010-0 */
#define TIMER_FREQ	1193182L
#define HZ		1000 

#include "type.h"

void	timecounter_inc();
size_t	clock();
void 	milli_delay(int milli_sec);

#endif