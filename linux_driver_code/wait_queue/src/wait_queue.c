#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

struct mydev {
	wait_queue_head_t wq;
	struct work_struct work;
	int condition;
};

struct mydev *dev;

static void work_handler(struct work_struct *work)
{
	pr_info("%s: Waitqueue module handler\n", __func__);
	
	msleep(5000);
	pr_info("%s: Wake up the sleeping module\n", __func__);
	
	dev->condition = 1;
	wake_up_interruptible(&dev->wq);
}

static int __init my_init(void)
{
	pr_info("%s ...\n", __func__);
	
	dev = kzalloc(sizeof(struct mydev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	
	INIT_WORK(&dev->work, work_handler);
	schedule_work(&dev->work);
	
	pr_info("%s: Going to sleep\n", __func__);
	
	init_waitqueue_head(&dev->wq);
	wait_event_interruptible(dev->wq, dev->condition != 0);
	
	pr_info("%s: woken up by the work job\n", __func__);
	
	return 0;
}

static void __exit my_exit(void)
{
	pr_info("%s: waitqueue example cleanup\n", __func__);
	
	if (dev)
		kfree(dev);
}

module_init(my_init);
module_exit(my_exit);
MODULE_AUTHOR("Yannik Li <yannik520@gmail.com>");
MODULE_LICENSE("GPL");








