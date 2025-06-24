#include <linux/device.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ipc_namespace.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/msg.h>
#include <linux/pgtable.h>
#include <linux/pipe_fs_i.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "lkm_def.c"

static int lkm_major;
static struct class *lkm_class;
static struct device *lkm_device;

static int lkm_init(void);
static void lkm_exit(void);
int lkm_open(struct inode *, struct file *);
int lkm_release(struct inode *, struct file *);
long lkm_ioctl(struct file *, unsigned int, long unsigned int);

static struct file_operations lkm_fops = {
	.owner = THIS_MODULE,
	.open = lkm_open,
	.release = lkm_release,
	.unlocked_ioctl = lkm_ioctl,
};

static int
lkm_init(void)
{
	printk(KERN_INFO "lkm: init: start\n");
	int ret;

	ret = register_chrdev(0, LKM_DEVICE_NAME, &lkm_fops);
	if (ret < 0) {
		printk(KERN_ERR "lkm: init: register chrdev failed\n");
		return ret;
	}

	lkm_major = ret;
	lkm_class = class_create(LKM_CLASS);

	if (IS_ERR(lkm_class)) {
		ret = PTR_ERR(lkm_device);
		printk(KERN_ERR "lkm: init: create class failed\n");
		unregister_chrdev(lkm_major, LKM_CLASS);
		return ret;
	}

	lkm_device = device_create(lkm_class, 0, MKDEV(lkm_major, 0), 0,
	    LKM_DEVICE_NAME);
	if (IS_ERR(lkm_device)) {
		ret = PTR_ERR(lkm_device);
		printk(KERN_ERR "lkm: init: create device failed\n");
		class_unregister(lkm_class);
		class_destroy(lkm_class);
		unregister_chrdev(lkm_major, LKM_CLASS);
		return ret;
	}

	printk(KERN_INFO "lkm: device driver created\n");

	printk(KERN_INFO "lkm: init: done\n");

	return 0;
}

static void
lkm_exit(void)
{
	printk(KERN_INFO "lkm: exit: start\n");

	device_destroy(lkm_class, MKDEV(lkm_major, 0));
	class_unregister(lkm_class);
	unregister_chrdev(lkm_major, LKM_DEVICE_NAME);

	printk(KERN_INFO "lkm: exit: done\n");
}

int
lkm_open(struct inode *inode, struct file *fp)
{
	printk(KERN_INFO "lkm: open: start\n");

	try_module_get(THIS_MODULE);

	printk(KERN_INFO "lkm: open: done\n");
	return 0;
}

int
lkm_release(struct inode *inode, struct file *fp)
{
	printk(KERN_INFO "lkm: release: start\n");

	module_put(THIS_MODULE);

	printk(KERN_INFO "lkm: release: done\n");
	return 0;
}

long
lkm_ioctl(struct file *fp, unsigned num, long unsigned int param)
{
	printk(KERN_INFO "lkm: ioctl: start\n");

	int ret;
	lkm_msg_t msg;
	size_t tmp;
	size_t *uaddr;

	ret = copy_from_user(&msg, (lkm_msg_t *)param, sizeof(lkm_msg_t));
	if (ret < 0) {
		printk(KERN_ERR "lkm: ioctl: copy_from_user failed\n");
		return -1;
	}

	switch (num) {
	case LKM_DPM_LEAK:
		tmp = (size_t)__va(0);
		uaddr = (size_t *)msg.drd.uaddr;
		printk(KERN_INFO "lkm: ioctl: dpm_leak: %lx\n", tmp);

		ret = copy_to_user(uaddr, &tmp, sizeof(size_t));
		if (ret < 0) {
			printk(KERN_ERR "lkm: ioctl: copy_to_user failed\n");
			return -1;
		}
		break;
	case LKM_STACK_LEAK:
		tmp = (size_t)current->stack;
		uaddr = (size_t *)msg.drd.uaddr;
		printk(KERN_INFO "lkm: ioctl: stack leak: %lx\n", tmp);

		ret = copy_to_user(uaddr, &tmp, sizeof(size_t));
		if (ret < 0) {
			printk(KERN_ERR "lkm: ioctl: copy_to_user failed\n");
			return -1;
		}
		break;
	case LKM_PIPE_BUFFER_LEAK:
		size_t id = msg.pbrd.fd;
		uaddr = (size_t *)msg.pbrd.uaddr;
		struct files_struct *files = current->files;

		spin_lock(&files->file_lock);
		struct fdtable *fdt = files_fdtable(files);
		struct file *file = fdt->fd[id];
		spin_unlock(&files->file_lock);

		struct pipe_inode_info *pipe = file->private_data;

		tmp = (size_t)&pipe->bufs[pipe->tail & (pipe->ring_size - 1)];

		printk(KERN_INFO "lkm: ioctl: pipe_buffer leak: %lx\n", tmp);

		ret = copy_to_user(uaddr, &tmp, sizeof(size_t));
		if (ret < 0) {
			printk(KERN_ERR "lkm: ioctl: copy_to_user failed\n");
			return -1;
		}

		break;
	default:
		printk(KERN_ERR "lkm: ioctl: unknown num: %u\n", num);
		return -1;
	}

	printk(KERN_INFO "lkm: ioctl: done\n");
	return 0;
}

MODULE_AUTHOR("Mira Chacku Purakal");
MODULE_DESCRIPTION("Generic Module");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");

module_init(lkm_init);
module_exit(lkm_exit);
