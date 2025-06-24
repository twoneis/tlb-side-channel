#pragma once

#define _GNU_SOURCE

#include <fcntl.h>

#include "timer.c"
#include "utils.c"

char buffer[0x100];

size_t
timed_alloc_primitive(int pipe[2])
{
	if (pipe2(pipe, O_NONBLOCK) < 0) {
		error("error creating pipe");
	}

	size_t time = rdtsc_begin();

	if (fcntl(pipe[0], F_SETPIPE_SZ, 8192) < 0) {
		error("error resizing pipe");
	}

	time = rdtsc_end() - time;

	if (write(pipe[1], buffer, 8) < 0) {
		error("error writing pipe");
	}

	return time;
}

void
access_primitive(int fd)
{
	__attribute__((unused)) int __ret = read(fd, (void *)0xdeadbeef000, 8);
}
