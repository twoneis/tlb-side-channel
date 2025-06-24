#pragma once
#include <stddef.h>

inline size_t
rdtsc_begin(void)
{
	size_t a, d;
	asm volatile("mfence");
	asm volatile("rdtsc" : "=a"(a), "=d"(d));
	a = (d << 32) | a;
	asm volatile("lfence");
	return a;
}

inline size_t
rdtsc_end(void)
{
	size_t a, d;
	asm volatile("lfence");
	asm volatile("rdtsc" : "=a"(a), "=d"(d));
	a = (d << 32) | a;
	asm volatile("mfence");
	return a;
}
