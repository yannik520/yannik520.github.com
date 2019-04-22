#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>

char tasklet_data[] = "tasklet example data";

static void tasklet_work(unsigned long data)
{
	pr_info("%s: data -> %s\n", __func__, (char *)data);
}

DECLARE_TASKLET(my_tasklet, tasklet_work, (unsigned long)tasklet_data);

static int __init my_init(void)
{
	pr_info("%s ...\n", __func__);
	
	tasklet_schedule(&my_tasklet);
	return 0;
}

static void __exit my_exit(void)
{
	pr_info("%s: ...\n", __func__);
	tasklet_kill(&my_tasklet);
}

module_init(my_init);
module_exit(my_exit);
MODULE_AUTHOR("Yannik Li <yannik520@gmail.com>");
MODULE_LICENSE("GPL");








