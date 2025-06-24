#pragma once
#include <sys/ioctl.h>

#include <fcntl.h>
#include <stdio.h>

#include "lkm_def.c"
#include "utils.c"

static int lkm_fd = -1;

void
lkm_open()
{
	if (lkm_fd != -1) {
		printf("lkm device already open");
		return;
	}

	lkm_fd = open(LKM_DEVICE, O_RDWR);

	if (lkm_fd < 0) {
		error("error opening lkm device");
	}
}

size_t
lkm_get_dpm_base()
{
	size_t dpm_base;
	lkm_msg_t msg = { .drd = { .uaddr = (size_t)&dpm_base } };
	if (ioctl(lkm_fd, LKM_DPM_LEAK, (unsigned long)&msg) < 0) {
		error("error in ioctl call for dpm base leak");
	}

	return dpm_base;
}

size_t
lkm_get_stack_base()
{
	size_t stack;
	lkm_msg_t msg = { .drd = { .uaddr = (size_t)&stack } };
	if (ioctl(lkm_fd, LKM_STACK_LEAK, (size_t)&msg) < 0) {
		error("error in ioctl call for stack base leak");
	}

	return stack;
}

size_t
lkm_pipe_buffer_leak(size_t fd)
{
	size_t pipe_buffer;
	lkm_msg_t msg = { .pbrd = { .uaddr = (size_t)&pipe_buffer, .fd = fd } };
	if (ioctl(lkm_fd, LKM_PIPE_BUFFER_LEAK, (size_t)&msg) < 0) {
		error("error in ioctl call for pipe buffer leak");
	}

	return pipe_buffer;
}
