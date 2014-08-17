#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>

#define VERSION "2.0"

#define DEBUG

#ifdef DEBUG
#define DBG(args...)	printk(args)
#else
#define DBG(args...)	do { } while(0)
#endif

#define MOVE_DEFAULT_SPEED      2
#define MOVE_MAX_SPEED          64
#define MOVE_MIN_SPEED          1
#define WHEEL_DEFAULT_SPEED     5
#define WHEEL_MAX_SPEED         7
#define WHEEL_MIN_SPEED         1
#define STEP_DEFAULT_VALUE	1
#define STEP_GAIN_VALUE		2

enum {
	KEY_VAL_UP,
	KEY_VAL_DOWN,
	KEY_VAL_PRESS,
};

struct key_status
{
	int	ctrl_key_down;
	int	alt_key_down;
	int	capslock_key_down;
};

static struct vmouse
{
        struct input_handler	*handler;
        struct input_handle	*handle;
        struct input_dev	*dev;
        struct work_struct	 work;
        int			 value;
        unsigned int		 event_code;
        unsigned int		 move_speed;
	unsigned int		 wheel_speed;
	bool			 keep_speed;
	struct key_status	 key_status;
        spinlock_t		 lock;
}*vms;

#define check_val(val, max_val, min_val) do {                   \
		(val) = (val) < (min_val) ? (min_val) :         \
			(val) > (max_val) ? (max_val) : (val);  \
	}while(0)

static ssize_t vmouse_wheel_speed_store(struct device *dev, struct device_attribute *attr, const char *buffer, size_t count)
{
	int wheel_speed;

	sscanf(buffer,"%d", &wheel_speed);
	DBG(KERN_INFO"mouse wheel speed changed to %d (max:%d min:%d)\n",
	       wheel_speed, WHEEL_MAX_SPEED, WHEEL_MIN_SPEED);

	check_val(wheel_speed, WHEEL_MAX_SPEED, WHEEL_MIN_SPEED);

	spin_lock(&vms->lock);
	vms->wheel_speed = wheel_speed;
	spin_unlock(&vms->lock);

	return count;
}

static ssize_t vmouse_wheel_speed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u",vms->wheel_speed);
}

DEVICE_ATTR(wheel_speed, 0644, vmouse_wheel_speed_show, vmouse_wheel_speed_store);

static void gearbox(int key_value, unsigned int *speed, bool keep_speed)
{
	static int	count = 0;
	static int	step  = STEP_DEFAULT_VALUE;
	
	switch(key_value)
	{
	case KEY_VAL_UP:
		count = 0;
		break;
	case KEY_VAL_DOWN:
		count = 0;
		if (!keep_speed)
		{
			*speed = MOVE_DEFAULT_SPEED;
			step = STEP_DEFAULT_VALUE;
		}
		break;
	case KEY_VAL_PRESS:
		if (!keep_speed)
		{
			count++;
			if (count == step)
			{
				if (*speed < MOVE_MAX_SPEED)
				{
					*speed =  *speed << 1;
					step += STEP_GAIN_VALUE;
				}
				count = 0;
			}
		}
		break;
	}

}

static void vms_func(struct work_struct *work)
{
        spin_lock(&vms->lock);

        switch(vms->event_code)
        {
	case KEY_P:
		gearbox(vms->value, &vms->move_speed, vms->keep_speed);
		input_report_rel(vms->dev, REL_X, 0);
		input_report_rel(vms->dev, REL_Y, -vms->move_speed);
		input_sync(vms->dev);
		break;
	case KEY_N:
		gearbox(vms->value, &vms->move_speed, vms->keep_speed);
		input_report_rel(vms->dev, REL_X, 0);
		input_report_rel(vms->dev, REL_Y, vms->move_speed);
		input_sync(vms->dev);
		break;
	case KEY_B:
		gearbox(vms->value, &vms->move_speed, vms->keep_speed);
		input_report_rel(vms->dev, REL_X, -vms->move_speed);
		input_report_rel(vms->dev, REL_Y, 0);
		input_sync(vms->dev);
		break;
	case KEY_F:
		gearbox(vms->value, &vms->move_speed, vms->keep_speed);
		input_report_rel(vms->dev, REL_X, vms->move_speed);
		input_report_rel(vms->dev, REL_Y, 0);
		input_sync(vms->dev);
		break;
	case KEY_V:
		input_report_rel(vms->dev, REL_WHEEL, -vms->wheel_speed);
		input_sync(vms->dev);
		break;
	case KEY_U:
		input_report_rel(vms->dev, REL_WHEEL, vms->wheel_speed);
		input_sync(vms->dev);
		break;
	case KEY_SPACE:
		input_report_key(vms->dev, BTN_LEFT, vms->value);
		input_sync(vms->dev);
		break;
	case KEY_M:
		input_report_key(vms->dev, BTN_RIGHT, vms->value);
		input_sync(vms->dev);
		break;
        }

        spin_unlock(&vms->lock);
}

static bool vkbd_filter(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	struct key_status *key = &vms->key_status;

        if (type == EV_KEY)
        {
		switch(code)
		{
		case KEY_LEFTCTRL:
			key->ctrl_key_down = value;
			if (vms->keep_speed)
			{
				return true;
			}
			break;
		case KEY_LEFTALT:
			key->alt_key_down = value;
			if (vms->keep_speed)
			{
				return true;
			}
			break;
		case KEY_CAPSLOCK:
			if (value == KEY_VAL_DOWN)
			{
				key->capslock_key_down = key->capslock_key_down ? 0 : 1;
			}
			spin_lock(&vms->lock);
			vms->keep_speed = key->capslock_key_down ? true : false;
			spin_unlock(&vms->lock);
			break;
		default:
			break;
		}

		if (key->ctrl_key_down && key->alt_key_down)
		{
			vms->event_code = code;
			vms->value	= value;

			schedule_work(&vms->work);

			switch(code)
			{
			case KEY_P:
			case KEY_N:
			case KEY_B:
			case KEY_F:
			case KEY_SPACE:
			case KEY_M:
			case KEY_V:
			case KEY_U:
				return true;
			default:
				return false;
			}
			DBG("%u, %u\n", code, value);
		}
        }
	return false;
}

static int vkbd_connect(struct input_handler *handler, struct input_dev *dev,
                        const struct input_device_id *id)
{
        struct input_handle	*handle;
        int			 error;
        int			 i;
       
        for (i = KEY_RESERVED; i < BTN_MISC; i++)
                if (test_bit(i, dev->keybit))
                        break;
       
        if (i == BTN_MISC && !test_bit(EV_SND, dev->evbit))
                return -ENODEV;
       
        handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
        vms->handle = handle;
        if (!handle)
                return -ENOMEM;
       
        handle->dev	= dev;
        handle->handler = handler;
        handle->name	= "vkbd";

        error = input_register_handle(handle);
        if (error)
                goto err_register_handle;
       
        error = input_open_device(handle);
        if (error)
                goto err_open_handle;
       
        DBG(KERN_INFO"vkbd_connect: %s\n", dev->name);
        return 0;

err_open_handle:
        input_unregister_handle(handle);
err_register_handle:
        kfree(handle);
        return error;
}

static void vkbd_disconnect(struct input_handle *handle)
{
        input_close_device(handle);
        input_unregister_handle(handle);
        kfree(handle);
}

static void vkbd_start(struct input_handle *handle)
{
        input_inject_event(handle, EV_LED, LED_CAPSL, 0x01);
        input_inject_event(handle, EV_SYN, SYN_REPORT, 0);
}

static const struct input_device_id vkbd_ids[] =
{
        {
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
        },
        {
        },
};

static struct input_handler vkbd_handler =
{
        .connect    = vkbd_connect,
        .disconnect = vkbd_disconnect,
        .start      = vkbd_start,
	.filter	    = vkbd_filter,
        .name       = "vkbd",
        .id_table   = vkbd_ids,
};

static int __init vmouse_init(void)
{
        int error;

        vms		 = kzalloc(sizeof(struct vmouse),GFP_KERNEL);
        vms->handler	 = &vkbd_handler;
        vms->move_speed  = MOVE_DEFAULT_SPEED;
	vms->wheel_speed = WHEEL_DEFAULT_SPEED;

	INIT_WORK(&vms->work,vms_func);
	spin_lock_init(&vms->lock);

        error = input_register_handler(&vkbd_handler);
        if (error)
                return error;

        vms->dev = input_allocate_device();
        if (!vms->dev)
        {
                printk(KERN_ERR"vmouse: failed to allocate the input device.\n");
                return -1;
        }

        vms->dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	vms->dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) | BIT_MASK(BTN_RIGHT);
        vms->dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y) | BIT_MASK(REL_WHEEL);
       
        vms->dev->name = "vmouse";
        vms->dev->phys = "vmouse";

        input_register_device(vms->dev);
        sysfs_create_file(&vms->dev->dev.kobj, (const struct attribute *)&dev_attr_wheel_speed);
       
        DBG(KERN_INFO"vmouse installed\n");
        return 0;
}

static void __exit vmouse_exit(void)
{
        sysfs_remove_file(&vms->dev->dev.kobj, (const struct attribute *)&dev_attr_wheel_speed);
        if (vms->dev)
		input_unregister_device(vms->dev);

        input_unregister_handler(vms->handler);
        kfree(vms);
}

module_init(vmouse_init);
module_exit(vmouse_exit);

MODULE_AUTHOR("Yanqing Li <yannik520@gmail.com>");
MODULE_DESCRIPTION("Virtual Mouse Driver Ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("Dual BSD/GPL");
