#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/shmem_fs.h>
#include <linux/mm_types.h>

struct shmem_device {
	struct file *file;
	struct page *page;
	size_t count;
};

static struct shmem_device *smdev;

static struct shmem_device *smdev_create(void)
{
	struct address_space *shmem_space;
	struct file *file;
	struct page *page;
	struct shmem_device *_smdev;
	
	_smdev = kzalloc(sizeof(*_smdev), GFP_KERNEL);
	if (!_smdev) {
		pr_err("alloc struct shmem_device failed\n");
		return ERR_PTR(-ENOMEM);
	}
	
	file = shmem_file_setup("shmem device",
				1 << PAGE_SHIFT,
				0);
	if (IS_ERR(file)) {
		pr_err("Failed allocating shmem device\n");
		kfree(_smdev);
		return ERR_CAST(file);
	}

	_smdev->file = file;
	shmem_space = file->f_mapping;
	
	page = shmem_read_mapping_page(shmem_space, 0);
	if (IS_ERR(page)) {
		pr_err("Failed allocating shmem page\n");
		fput(_smdev->file);
		kfree(_smdev);
		return ERR_CAST(page);
	}
	_smdev->page = page;
	
	printk("%s: file count=%ld\n", __func__, file_count(file));
	return _smdev;
}

static void smdev_free(struct shmem_device *_smdev)
{
	if (IS_ERR(_smdev))
		return;

	if (_smdev->page)
		put_page(_smdev->page);
	if (_smdev->file)
		fput(_smdev->file);
	kfree(_smdev);
}

static int smdev_open(struct inode *inode, struct file *filp)
{
	if (IS_ERR(smdev))
		return PTR_ERR(smdev);

	filp->private_data = smdev;
	return 0;
}

static int smdev_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t smdev_read(struct file *filp, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct shmem_device *_smdev = filp->private_data;
	struct page *shmem_page = _smdev->page;
	size_t size = min(_smdev->count, count);
	int ret;
	printk("%s: data_size=%ld\n", __func__, size);
	
	if (copy_to_user(buf, page_to_virt(shmem_page), size)) {
		ret = -EFAULT;
	} else {
		ret = size;
	}
	
	return ret;
}

static ssize_t smdev_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct shmem_device *_smdev = filp->private_data;
	struct page *shmem_page = _smdev->page;
	int ret;
	
	printk("%s: count=%ld\n", __func__, count);
	_smdev->count = count;
	if (copy_from_user(page_to_virt(shmem_page), buf, count)) {
		ret = -EFAULT;
	} else {
		ret = count;
	}

	return ret;
}

static const struct file_operations smdev_fops = {
	.owner	 = THIS_MODULE,
	.open    = smdev_open,
	.read    = smdev_read,
	.write   = smdev_write,
	.release = smdev_close,
	.llseek  = no_llseek,
};

static char *smdev_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0666;
	return kasprintf(GFP_KERNEL, "smdev/%s", dev_name(dev));
}

static struct class *smdev_class;
int major = 0;
int minor = 0;

static int __init smdev_init(void)
{
	smdev = smdev_create();
	if (IS_ERR(smdev))
		return PTR_ERR(smdev);
	
	major = register_chrdev(0, "smdev", &smdev_fops);
	if (major < 0) {
		pr_err("register shmem character device failed\n");
		return major;
	}
	
	smdev_class = class_create(THIS_MODULE, "smdev");
	if (IS_ERR(smdev_class))
		return PTR_ERR(smdev_class);
	
	smdev_class->devnode = smdev_devnode;
	
	device_create(smdev_class, NULL, MKDEV(major, minor),
		      NULL, "smdev%d", minor);
	
	return 0;
}

static void __exit smdev_exit(void)
{
	device_destroy(smdev_class, MKDEV(major, minor));
	class_destroy(smdev_class);
	unregister_chrdev(major, "smdev");
	smdev_free(smdev);
}

module_init(smdev_init);
module_exit(smdev_exit);

MODULE_AUTHOR("Yannik Li");
MODULE_LICENSE("GPL");
