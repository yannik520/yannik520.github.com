#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

struct workqueue_struct *wq;
struct work_data {
	struct work_struct my_work;
	int data;
};

static void work_handler(struct work_struct *work)
{
	struct work_data *my_data = container_of(work,
						 struct work_data, my_work);

	pr_info("%s: data = %d\n", __func__, my_data->data);
	kfree(my_data);
}

static int __init my_init(void)
{
	struct work_data *my_data;
	
	pr_info("%s ...\n", __func__);

	my_data = kmalloc(sizeof(struct work_data), GFP_KERNEL);
	if (!my_data)
		return -ENOMEM;
	my_data->data = 55;

	wq = create_singlethread_workqueue("my_single_thread");
	if (!wq)
		goto create_workqueue_error;
	
	INIT_WORK(&my_data->my_work, work_handler);
	queue_work(wq, &my_data->my_work);
	
	return 0;

create_workqueue_error:
	kfree(my_data);
	return -ENOMEM;
}

static void __exit my_exit(void)
{
	if (wq) {
		flush_workqueue(wq);
		destroy_workqueue(wq);
	}
	
	pr_info("%s: module exit\n", __func__);
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yannik Li <yannik520@gmail.com>");
