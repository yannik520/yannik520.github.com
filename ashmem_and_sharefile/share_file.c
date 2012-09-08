#include <asm/cacheflush.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nsproxy.h>
#include <linux/poll.h>
#include <linux/debugfs.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include "share_file.h"

struct shfile_proc
{
	int pid;
	struct task_struct *tsk;
	struct files_struct *files;
};

struct shfile_file
{
	int fd;
	struct file *fp;
};

struct shfile_file shfile;

static int shfile_open(struct inode *inode, struct file *file)
{
	struct shfile_proc *proc;

	proc = kzalloc(sizeof(*proc), GFP_KERNEL);
	if (proc == NULL)
		return -ENOMEM;
	proc->tsk = current;
	proc->pid = current->group_leader->pid;
	proc->files = get_files_struct(proc->tsk);
	file->private_data = proc;
	
	return 0;
}

static int shfile_release(struct inode *inode, struct file *file)
{
	struct shfile_proc *proc = file->private_data;

	kfree(proc);
	return 0;
}

static int task_get_unused_fd_flags(struct shfile_proc *proc, int flags)
{
	struct files_struct *files = proc->files;
	int fd, error;
	struct fdtable *fdt;
	unsigned long rlim_cur;
	unsigned long irqs;

	if (files == NULL)
		return -ESRCH;

	error = -EMFILE;
	spin_lock(&files->file_lock);

repeat:
	fdt = files_fdtable(files);
	fd = find_next_zero_bit(fdt->open_fds, fdt->max_fds, files->next_fd);

	/*
	 * N.B. For clone tasks sharing a files structure, this test
	 * will limit the total number of files that can be opened.
	 */
	rlim_cur = 0;
	if (lock_task_sighand(proc->tsk, &irqs)) {
		rlim_cur = proc->tsk->signal->rlim[RLIMIT_NOFILE].rlim_cur;
		unlock_task_sighand(proc->tsk, &irqs);
	}
	if (fd >= rlim_cur)
		goto out;

	/* Do we need to expand the fd array or fd set?  */
	error = expand_files(files, fd);
	if (error < 0)
		goto out;

	if (error) {
		/*
		 * If we needed to expand the fs array we
		 * might have blocked - try again.
		 */
		error = -EMFILE;
		goto repeat;
	}

	__set_open_fd(fd, fdt);
	if (flags & O_CLOEXEC)
		__set_close_on_exec(fd, fdt);
	else
		__clear_close_on_exec(fd, fdt);
	files->next_fd = fd + 1;
#if 1
	/* Sanity check */
	if (fdt->fd[fd] != NULL) {
		printk(KERN_WARNING "get_unused_fd: slot %d not NULL!\n", fd);
		fdt->fd[fd] = NULL;
	}
#endif
	error = fd;

out:
	spin_unlock(&files->file_lock);
	return error;
}

static void task_fd_install(
	struct shfile_proc *proc, unsigned int fd, struct file *file)
{
	struct files_struct *files = proc->files;
	struct fdtable *fdt;

	if (files == NULL)
		return;

	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	BUG_ON(fdt->fd[fd] != NULL);
	rcu_assign_pointer(fdt->fd[fd], file);
	spin_unlock(&files->file_lock);
}

static long shfile_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct shfile_proc *proc = file->private_data;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	switch (cmd)
	{
	case SHFILE_SHARE_FD:
	{
		int fd;
		struct file *file;

		if (size != sizeof(int))
		{
			ret = -EINVAL;
			goto err;
		}
		if (copy_from_user(&fd, ubuf, size))
		{
			ret = -EFAULT;
			goto err;
		}
		
		file = fget(fd);
		if (file == NULL)
		{
			printk(KERN_ERR "shfile: invalid fd, %d\n", fd);
			ret = -EINVAL;
			goto err;
		}
		
		shfile.fd = fd;
		shfile.fp = file;
		break;
	}
	case SHFILE_GET_FD:
	{
		int target_fd;
		struct file *file;

		if (size != sizeof(int))
		{
			ret = -EINVAL;
			goto err;
		}
		target_fd = task_get_unused_fd_flags(proc, O_CLOEXEC);
		if (target_fd < 0) {
			ret = -EFAULT;
			goto err;
		}
		file = shfile.fp;
		task_fd_install(proc, target_fd, file);
			
		if (copy_to_user(ubuf, &target_fd, size))
		{
			ret = -EFAULT;
			goto err;
		}
		break;
	}
	default:
		printk(KERN_INFO "shfile: invalid command!\n");
		break;
	}

	return 0;
err:
	return ret;

}


static const struct file_operations shfile_fops = {
	.owner = THIS_MODULE,
	.open = shfile_open,
	.release = shfile_release,
	.unlocked_ioctl = shfile_ioctl,
	.compat_ioctl = shfile_ioctl,
};

static struct miscdevice shfile_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "shfile",
	.fops = &shfile_fops,
};

static int __init shfile_init(void)
{
	int ret;

	ret = misc_register(&shfile_misc);
	if (unlikely(ret))
	{
		printk(KERN_ERR "shfile: failed to register misc device!\n");
	}

	printk(KERN_INFO "shfile: initialized\n");

	return ret;
}

static void __exit shfile_exit(void)
{
	int ret;
	
	ret = misc_deregister(&shfile_misc);
	if (unlikely(ret))
		printk(KERN_ERR "shfile: failed to unregister misc device!\n");
	
	printk(KERN_INFO "shfile: unloaded\n");

}

module_init(shfile_init);
module_exit(shfile_exit);

MODULE_LICENSE("GPL");
