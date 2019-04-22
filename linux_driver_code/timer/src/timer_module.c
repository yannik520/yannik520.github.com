#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>

static struct timer_list my_timer;

static void my_timer_callback(struct timer_list *timer)
{
	printk("%s called (%ld)\n", __func__, jiffies);
}

static int __init my_init(void)
{
	int ret;
	
	pr_info("%s: Timer module loaded\n", __func__);
	
	timer_setup(&my_timer, my_timer_callback, 0);
	pr_info("%s: Setup timer to fire in 2s (%ld)\n", __func__, jiffies);

	ret = mod_timer(&my_timer, jiffies + msecs_to_jiffies(2000));
	if (ret)
		pr_err("%s: Timer firing failed\n", __func__);
	
	return 0;
}

static void __exit my_exit(void)
{
	int ret;
	
	ret = del_timer(&my_timer);
	if (ret)
		pr_err("%s: The timer is still is use ...\n", __func__);
	
	pr_info("%s: Timer module unloaded\n", __func__);
}

module_init(my_init);
module_exit(my_exit);
MODULE_AUTHOR("Yannik Li <yannik520@gmail.com>");
MODULE_DESCRIPTION("Timer example");
MODULE_LICENSE("GPL");
