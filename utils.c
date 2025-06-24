#pragma once
#define _GNU_SOURCE
#include <sys/resource.h>
#include <sys/socket.h>

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

void
error(char *origin)
{
	printf("[!] %s: ", origin);
	switch (errno) {
	case EFAULT:
		printf("EFAULT\n");
		break;
	case EINVAL:
		printf("EINVAL\n");
		break;
	case EMFILE:
		printf("EMFILE\n");
		break;
	case ENFILE:
		printf("ENFILE\n");
		break;
	case EACCES:
		printf("EACCES\n");
		break;
	case EAFNOSUPPORT:
		printf("EAFNOSUPPORT\n");
		break;
	case ENOBUFS:
		printf("ENOBUFS\n");
		break;
	case ENOMEM:
		printf("ENOMEM\n");
		break;
	case EPROTONOSUPPORT:
		printf("EPROTONOSUPPORT\n");
		break;
	default:
		printf("unknown errno: %i\n", errno);
	}
	exit(-1);
}

void
set_limit(void)
{
	struct rlimit l = {
		.rlim_cur = 100000,
		.rlim_max = 100000,
	};

	if (setrlimit(RLIMIT_NOFILE, &l) < 0) {
		error("error when setting limit");
	}
}

void
pin_to_core(size_t core)
{
	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	if (sched_setaffinity(0, sizeof(cpuset), &cpuset)) {
		error("error pinning to core");
	}
}

size_t
get_total_memory(void)
{
	FILE *fp;
	char input[100];
	size_t mem_total_kb;
	size_t mem_total_rounded = 0;

	fp = popen("awk '/MemTotal/ { print $2 }' /proc/meminfo", "r");
	if (!fp) {
		error("error reading from /proc/meminfo");
	}

	if (fgets(input, sizeof(input), fp) != NULL) {
		mem_total_kb = atoi(input);
		if (mem_total_kb == 0) {
			error("failed to convert MemTotal");
		}
		mem_total_rounded = (mem_total_kb + (1 << 20) - 1) / (1 << 20)
		    << 30;
	} else {
		error("failed to get MemTotal");
	}

	pclose(fp);

	return mem_total_rounded;
}
