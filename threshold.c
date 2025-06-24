#define _GNU_SOURCE
#include <sys/ioctl.h>

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <threads.h>

#include "lkm_utils.c"
#include "prefetch.c"
#include "timer.c"
#include "tlbflush.c"
#include "utils.c"

typedef struct dual_threshold {
	unsigned lower;
	unsigned upper;
} dual_threshold_t;

#define HIST_SIZE_THRESHOLD 100
#define THRESHOLD_TRIES	    500

dual_threshold_t __attribute__((noinline, aligned(4096)))
detect_threshold_single(size_t addr_mapped, size_t addr_unmapped)
{
	const unsigned reps = 10000;
	size_t time_m, time_um;
	size_t hist_m[HIST_SIZE_THRESHOLD] = { 0 };
	size_t hist_um[HIST_SIZE_THRESHOLD] = { 0 };

	for (size_t i = 0; i < reps; i++) {
		prefetch((void *)addr_mapped);
		asm volatile("lfence");
		asm volatile("mfence");

		time_m = rdtsc_begin();
		prefetch((void *)addr_mapped);
		time_m = rdtsc_end() - time_m;

		time_um = rdtsc_begin();
		prefetch((void *)addr_unmapped);
		time_um = rdtsc_end() - time_um;

		asm volatile("lfence");
		asm volatile("mfence");

		hist_m[MIN(HIST_SIZE_THRESHOLD - 2, time_m)]++;
		hist_um[MIN(HIST_SIZE_THRESHOLD - 2, time_um)]++;
	}

	size_t sum[2] = { 0 };

	unsigned max = 0, threshold_i = 0, limit1 = 0, limit2 = 0;

	for (size_t i = 0; i < HIST_SIZE_THRESHOLD; i += 2) {
		sum[0] += hist_m[i];
		sum[1] += hist_um[i];
		if ((sum[0] - sum[1]) > max) {
			max = (sum[0] - sum[1]);
		}
	}

	sum[0] = 0;
	sum[1] = 0;

	for (size_t i = 0; i < HIST_SIZE_THRESHOLD; i += 2) {
		sum[0] += hist_m[i];
		sum[1] += hist_um[i];
		if (!limit1 && (sum[0] - sum[1]) >= 0.97 * max) {
			limit1 = i;
		}
		if (limit1 && !limit2 && (sum[0] - sum[1]) <= 0.97 * max) {
			limit2 = i;
			threshold_i = (limit1 + limit2) / 2;
			threshold_i += threshold_i % 2;
		}
	}

	dual_threshold_t t = { limit1, limit2 };

	return t;
}

dual_threshold_t
detect_threshold(size_t addr_mapped, size_t addr_unmapped, const unsigned reps)
{
	size_t threshold_hist_lower[HIST_SIZE_THRESHOLD] = { 0 };
	size_t threshold_hist_upper[HIST_SIZE_THRESHOLD] = { 0 };

	for (unsigned i = 0; i < 20; i++) {
		detect_threshold_single(addr_mapped, addr_unmapped);
	}

	for (unsigned i = 0; i < reps; i++) {
		dual_threshold_t t = detect_threshold_single(addr_mapped,
		    addr_unmapped);
		threshold_hist_lower[t.lower]++;
		threshold_hist_upper[t.upper]++;
	}

	unsigned threshold_l = 0, threshold_l_i = 0, threshold_u = 0,
		 threshold_u_i = 0;
	for (size_t i = 0; i < HIST_SIZE_THRESHOLD; i += 2) {
		if (threshold_hist_lower[i] > threshold_l) {
			threshold_l = threshold_hist_lower[i];
			threshold_l_i = i;
		}
		if (threshold_hist_upper[i] > threshold_u) {
			threshold_u = threshold_hist_upper[i];
			threshold_u_i = i;
		}
	}

	dual_threshold_t t = { threshold_l_i, threshold_u_i };

	return t;
}

int
main(void)
{
	set_limit();
	lkm_open();
	pin_to_core(0);
	init_tlb_flush();

	size_t stack = lkm_get_stack_base();

	dual_threshold_t t = detect_threshold(stack + 0x3000, stack + 0x4000,
	    100);

	printf("main: detected thresholds: %d %d\n", t.lower,
	    (t.lower + t.upper * 2) / 3);

	threshold_hit_flush(stack + 0x300, THRESHOLD_TRIES);
}
