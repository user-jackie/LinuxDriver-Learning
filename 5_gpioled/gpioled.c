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

#define CCM_CCGR1_BASE              (0x020C406C)
#define SW_MUX_GPIO1_IO03_BASE      (0x020E0068)
#define SW_PAD_GPIO1_IO03_BASE      (0x020E02F4)
#define GPIO1_DR_BASE               (0x0209C000)
#define GPIO1_GDIR_BASE             (0x0209C004)

static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

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
};

struct gpioled_dev gpioled;

static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &gpioled;
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

static int led_release(struct inode *inode, struct file *filp)
{
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
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

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