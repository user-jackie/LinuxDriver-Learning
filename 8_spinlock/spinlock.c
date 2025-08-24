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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define gpioled_CNT   1
#define gpioled_NAME  "gpioled"
#define LEDOFF          0
#define LEDON           1

struct gpioled_dev
{
    dev_t devid;                /* 设备号 */
    struct cdev cdev;           /* cdev */
    struct class *class;        /* 类 */
    struct device *device;      /* 设备 */
    int major;                  /* 主设备号 */
    int minor;                  /* 次设备号 */
    struct device_node *nd;     /* 设备节点 */
    int led_gpio;               /* led所使用的GPIO编号 */
    int dev_stats;
    spinlock_t lock;
};

struct gpioled_dev gpioled;

/* App调用open函数时执行该函数 */
static int led_open(struct inode *inode, struct file *filp)
{
    unsigned long flags;            /* flags表示中断状态 */
    filp->private_data = &gpioled;

    /* 临界区 */
    spin_lock_irqsave(&gpioled.lock, flags);        /* 上锁 */
    if(gpioled.dev_stats) {
        spin_unlock_irqrestore(&gpioled.lock, flags);
        return -EBUSY;
    }
    gpioled.dev_stats ++;
    spin_unlock_irqrestore(&gpioled.lock, flags);

    return 0;
}

/*ssize_t 类型为 有符号整型，size_t 类型为 无符号整型*/
/*loff_t 类型为 long long 类型*/
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue = 0;
    unsigned char databuf[1];
    unsigned char ledstat;
    struct gpioled_dev *dev = filp->private_data;

    retvalue = copy_from_user(databuf, buf, cnt);
    if(retvalue < 0) {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledstat = databuf[0];

    if(ledstat == LEDON) {
        gpio_set_value(dev->led_gpio, 0);
    } else if (ledstat == LEDOFF) {
        gpio_set_value(dev->led_gpio, 1);
    }
    return 0;
}

/* 程序调用close函数关闭驱动文件，led_release函数执行 */
static int led_release(struct inode *inode, struct file *filp)
{
    unsigned long flags;
    /* filp->private_data为当前dev的值，需要进行修改(因为取地址)才设置的dev */
    struct gpioled_dev *dev = filp->private_data;
    
    spin_lock_irqsave(&dev->lock, flags);
    if(dev->dev_stats) {
        dev->dev_stats --;
    }
    spin_unlock_irqrestore(&dev->lock, flags);
    
    return 0;
}

static struct file_operations gpioled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

static int __init led_init(void)
{
    int ret = 0;

    spin_lock_init(&gpioled.lock);

    /* 获取设备树中的属性数据 */
    /* 1.获取设备节点:alphaled */
    gpioled.nd = of_find_node_by_path("/gpioled");
    if(gpioled.nd == NULL) {
        printk("alphaled node can not found!\r\n");
        return -EINVAL;
    } else {
        printk("alphaled node has been found!\r\n");
    }

    gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
    if(gpioled.led_gpio < 0) {
        printk("can't get led-gpio\r\n");
        return -EINVAL;
    }
    printk("led-gpio num = %d\r\n", gpioled.led_gpio);

    ret = gpio_direction_output(gpioled.led_gpio, 1);
    if(ret < 0) {
        printk("can't set gpio!\r\n");
    }

    /* 注册字符设备驱动 */
    /* 1.创建设备号 */
    if (gpioled.major) {
        gpioled.devid = MKDEV(gpioled.major, 0);
        register_chrdev_region(gpioled.devid, gpioled_CNT, gpioled_NAME);
    } else {
        alloc_chrdev_region(&gpioled.devid, 0, gpioled_CNT, gpioled_NAME);
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }
    printk("gpioled major=%d, minor=%d\r\n", gpioled.major, gpioled.minor);

    /* 2.初始化cdev */
    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev, &gpioled_fops);

    /* 3.添加一个cdev */
    cdev_add(&gpioled.cdev, gpioled.devid, gpioled_CNT);

    /* 4.创建类 */
    gpioled.class = class_create(THIS_MODULE, gpioled_NAME);
    if(IS_ERR(gpioled.class)) {
        return PTR_ERR(gpioled.class);
    }

    /* 5.创建设备 */
    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, gpioled_NAME);
    if (IS_ERR(gpioled.device)) {
        return PTR_ERR(gpioled.device);
    }

    return 0;
}

static void __exit led_exit(void)
{
    /* 1.删除设备节点:在/dev目录下删除节点 */
    device_destroy(gpioled.class, gpioled.devid);
    /* 2.删除设备类:在/sys/class/下删除设备类目录 */
    class_destroy(gpioled.class);
    /* 3.在内核中删除cdev设备 */
    cdev_del(&gpioled.cdev);
    /* 4.取消注册cdev结构体 */
    unregister_chrdev_region(gpioled.devid, gpioled_CNT);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liaoyuan");