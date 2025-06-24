#define LKM_DEVICE	     "/dev/lkm"
#define LKM_DEVICE_NAME	     "lkm"
#define LKM_CLASS	     "lkmclass"

#define LKM_DPM_LEAK	     101
#define LKM_STACK_LEAK	     102
#define LKM_PIPE_BUFFER_LEAK 103

typedef union {
	struct dpm_rd {
		size_t uaddr;
	} drd;
	struct pipe_buffer_rd {
		size_t fd;
		size_t uaddr;
	} pbrd;
} lkm_msg_t;
