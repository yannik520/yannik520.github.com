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
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>

#define MAX_PAGES 5

struct cdev_buffer {
	struct page **mapped_pages;
	unsigned long offset;
	int nr_pages;
};

static int cdev_map_user_pages(struct cdev_buffer *cbuf,
			      const unsigned int max_pages, unsigned long uaddr,
			      size_t count, int rw)
{
	unsigned long end = (uaddr + count + PAGE_SIZE - 1) >> PAGE_SHIFT;
	unsigned long start = uaddr >> PAGE_SHIFT;
	const int nr_pages = end - start;
	int res, i, j;
	struct page **pages;

	if ((uaddr + count) < uaddr)
		return -EINVAL;

        if (nr_pages > max_pages)
		return -ENOMEM;

	if (count == 0)
		return 0;

	if ((pages = kmalloc(max_pages * sizeof(*pages), GFP_KERNEL)) == NULL)
		return -ENOMEM;

        /* Try to fault in all of the necessary pages */
        /* rw==WRITE means write into memory area */
	res = get_user_pages_unlocked(
		uaddr,
		nr_pages,
		pages,
		rw == WRITE ? FOLL_WRITE : 0); /* don't force */

	/* Errors and no page mapped should return here */
	if (res < nr_pages)
		goto out_unmap;

        for (i = 0; i < nr_pages; i++) {
		flush_dcache_page(pages[i]);
        }

	cbuf->offset = uaddr & ~PAGE_MASK;
	cbuf->mapped_pages = pages;

	return nr_pages;
 out_unmap:
	if (res > 0) {
		for (j = 0; j < res; j++)
			put_page(pages[j]);
		res = 0;
	}
	kfree(pages);
	return res;
}

static int cdev_unmap_user_pages(struct cdev_buffer *cbuf,
				const unsigned int nr_pages, int dirtied)
{
	int i;

	for (i = 0; i < nr_pages; i++) {
		struct page *page = cbuf->mapped_pages[i];

		if (dirtied)
			SetPageDirty(page);

		put_page(page);
	}
	kfree(cbuf->mapped_pages);
	cbuf->mapped_pages = NULL;

	return 0;
}

static int cdev_open(struct inode *inode, struct file *filp)
{
	struct cdev_buffer *cbuf = NULL;

	cbuf = kzalloc(sizeof(struct cdev_buffer), GFP_KERNEL);
	if (!cbuf) {
		pr_err("%s: alloc cdev_buffer failed\n", __func__);
		return -1;
	}

	filp->private_data = cbuf;
	return 0;
}

static int cdev_close(struct inode *inode, struct file *filp)
{
	struct cdev_buffer *cbuf = filp->private_data;

	if (cbuf) {
		if (cbuf->nr_pages) {
			cdev_unmap_user_pages(cbuf, cbuf->nr_pages, true);
		}
		kfree(cbuf);
	}
	return 0;
}

static ssize_t cdev_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct cdev_buffer *cbuf = filp->private_data;
	struct page *page = cbuf->mapped_pages[0];
	unsigned long kaddr = (unsigned long)page_address(page) + cbuf->offset;

	pr_debug("%s: read data(%lu): %s\n", __func__, count, (char *)kaddr);

	return count - copy_to_user(buf, (const void *)kaddr, count);
}

static ssize_t cdev_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct cdev_buffer *cbuf = filp->private_data;
	int ret;

	ret = cdev_map_user_pages(cbuf, MAX_PAGES, (unsigned long)buf, count, WRITE);
	if (ret <= 0) {
		pr_err("%s: map user pages failed.\n", __func__);
		return 0;
	}

	cbuf->nr_pages = ret;

	return count;
}

static const struct file_operations cdev_fops = {
	.owner	 = THIS_MODULE,
	.open    = cdev_open,
	.read    = cdev_read,
	.write   = cdev_write,
	.release = cdev_close,
	.llseek  = no_llseek,
};

static char *cdev_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0666;
	return kasprintf(GFP_KERNEL, "cdev/%s", dev_name(dev));
}

static struct class *cdev_class;
int major = 0;
int minor = 0;

static int __init cdev_init(void)
{
	major = register_chrdev(0, "cdev", &cdev_fops);
	if (major < 0) {
		pr_err("register character device failed\n");
		return major;
	}

	cdev_class = class_create(THIS_MODULE, "cdev");
	if (IS_ERR(cdev_class))
		return PTR_ERR(cdev_class);

	cdev_class->devnode = cdev_devnode;

	device_create(cdev_class, NULL, MKDEV(major, minor),
		      NULL, "cdev%d", minor);

	return 0;
}

static void __exit cdev_exit(void)
{
	device_destroy(cdev_class, MKDEV(major, minor));
	class_destroy(cdev_class);
	unregister_chrdev(major, "cdev");
}

module_init(cdev_init);
module_exit(cdev_exit);

MODULE_AUTHOR("Yannik Li");
MODULE_LICENSE("GPL");
