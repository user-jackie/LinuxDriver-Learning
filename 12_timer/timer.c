#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/semaphore.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define TIMER_CNT   1
#define TIMER_NAME  "timerdev"
#define CLOSE_CMD   (_IO(0XEF, 0X1))
#define OPEN_CMD    (_IO(0XEF, 0X2))
#define SETPERIOD_CMD   (_IO(0XEF, 0X3))
#define LEDON       1
#define LEDOFF      0

struct timer_dev {
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
    int led_gpio;
    int timeperiod;             /* 定时周期 */
    struct timer_list timer;    /* 定义一个定时器 */
    spinlock_t lock;            /* 定义自旋锁 */
};

struct timer_dev timerdev;

/* 初始化led灯的IO，在open()函数打开驱动时调用 */
/* 从设备数获取信息，然后初始化相应的IO */
static int led_init(void)
{
    int ret;

    timerdev.nd = of_find_node_by_path("/gpioled");
    if(timerdev.nd == NULL) {
        printk("can't find node!\r\n");
        return -EINVAL;
    }

    timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio", 0);
    if(timerdev.led_gpio < 0) {
        printk("can't find led-gpio!\r\n");
        return -EINVAL;
    }

    gpio_request(timerdev.led_gpio, "led");
    ret = gpio_direction_output(timerdev.led_gpio, 1);
    if(ret < 0) {
        printk("can't set direction!\r\n");
        return -EINVAL;
    }

    return 0;
}

/* open函数用来设置初始化，设置private_data为&timerdev */
static int timer_open(struct inode *inode, struct file *filp)
{
    int ret;
    filp->private_data = &timerdev;

    timerdev.timeperiod = 1000;
    /* 调用一次初始化 */
    ret = led_init();
    if(ret < 0) {
        return ret;
    }

    return 0;
}

static long timer_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    /* 强转数据 */
    struct timer_dev *dev = (struct timer_dev *)filp->private_data;
    int timerperiod;
    unsigned long flags;        /* 中断状态 */

    switch (cmd)
    {
    case CLOSE_CMD:
        del_timer_sync(&dev->timer);    /* 此函数内部需要传递地址型数据 */
        break;
    case OPEN_CMD:
        /* dev->timerperiod的值可能会被其他程序引用或修改，设置自旋锁 */
        spin_lock_irqsave(&dev->lock, flags);
        timerperiod = dev->timeperiod;
        spin_unlock_irqrestore(&dev->lock, flags);
        /*
        * mod_timer:用于修改定时值，如果定时器还没有激活的话， mod_timer 函数会激活定时器
        * msecs_to_jiffies:将给定的毫秒数（msecs）转换为对应的时钟节拍数(jiffies)
        */
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
        break;
    case SETPERIOD_CMD:
        spin_lock_irqsave(&dev->lock, flags);
        dev->timeperiod = arg;
        spin_unlock_irqrestore(&dev->lock, flags);
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
        break;
    default:
        break;
    }
    return 0;
}

/* .unlocked_ioctl:被解锁的输入输出控制 */
static struct file_operations timer_fops = {
    .owner = THIS_MODULE,
    .open = timer_open,
    .unlocked_ioctl = timer_unlocked_ioctl,
};

/* 定时器回调函数:用于实现led灯翻转 */
/* arg参数为timerdev的地址 */
void timer_function(unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)arg;
    static int sta = 1;
    int timerperiod;
    unsigned long flags;

    sta = !sta;
    gpio_set_value(dev->led_gpio, sta);

    /* 由于内核的定时器不是循环的定时器，所以需要重启定时器 */
    /* 重启定时器 */
    spin_lock_irqsave(&dev->lock, flags);
    timerperiod = dev->timeperiod;
    spin_unlock_irqrestore(&dev->lock, flags);
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
}

static int __init timer_init(void)
{

    spin_lock_init(&timerdev.lock);

    if(timerdev.major) {
        timerdev.devid = MKDEV(timerdev.major, 0);
        register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
    } else {
        alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);
        timerdev.major = MAJOR(timerdev.devid);
        timerdev.minor = MINOR(timerdev.devid);
    }
    printk("major=%d, minor=%d", timerdev.major, timerdev.minor);

    /* THIS_MODULE定义为(struct module *)0 */
    timerdev.cdev.owner = THIS_MODULE;
    cdev_init(&timerdev.cdev, &timer_fops);

    cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);

    /* class_create返回值为class类型 */
    timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
    if(IS_ERR(timerdev.class)) {
        return PTR_ERR(timerdev.class);
    }

    timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
    if(IS_ERR(timerdev.device)) {
        return PTR_ERR(timerdev.class);
    }

    init_timer(&timerdev.timer);
    /* 设置定时器回调函数 */
    timerdev.timer.function = timer_function;
    /* 设置要传递给 timer_function 函数的参数为 timerdev 的地址 */
    timerdev.timer.data = (unsigned long)&timerdev;
    return 0;
}

static void __exit timer_exit(void)
{
    gpio_set_value(timerdev.led_gpio, 1);
    /* 同步删除:待其他处理器完成对定时器的操作后再进行删除 */
    del_timer_sync(&timerdev.timer);

    /* 删除一般从地址删除 */
    /* 设备为在class这个大类中的某个id:如gpio大类，device可能为1, 2, 3 ...所以删除时需要指明id号 */
    device_destroy(timerdev.class, timerdev.devid);
    class_destroy(timerdev.class);
    cdev_del(&timerdev.cdev);
    unregister_chrdev_region(timerdev.devid, TIMER_CNT);
}

module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liaoyuan");