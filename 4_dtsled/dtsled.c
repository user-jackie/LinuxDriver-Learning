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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define dtsled_CNT   1
#define dtsled_NAME  "dtsled"
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

struct dtsled_dev
{
    dev_t devid;                /* 设备号 */
    struct cdev cdev;           /* cdev */
    struct class *class;        /* 类 */
    struct device *device;      /* 设备 */
    int major;                  /* 主设备号 */
    int minor;                  /* 次设备号 */
    struct device_node *nd;     /* 设备节点 */
};

struct dtsled_dev dtsled;


void led_switch(u8 sta)
{
    u32 val = 0;
    if(sta == LEDON) {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);
        writel(val, GPIO1_DR);
    }else if(sta == LEDOFF) {
        val = readl(GPIO1_DR);
        val |= (1 << 3);
        writel(val, GPIO1_DR);
    }
}

static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &dtsled;
    return 0;
}

/*ssize_t 类型为 有符号整型，size_t 类型为 无符号整型*/
/*loff_t 类型为 long long 类型*/
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    u32 val = 0;
    val = readl(GPIO1_DR);
    val = (val << 3) & 0x1;
    return val;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue = 0;
    unsigned char databuf[1];
    unsigned char ledstat;

    retvalue = copy_from_user(databuf, buf, cnt);
    if(retvalue < 0) {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledstat = databuf[0];

    if(ledstat == LEDON) {
        led_switch(LEDON);
    } else if (ledstat == LEDOFF) {
        led_switch(LEDOFF);
    }
    return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations dtsled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

static int __init led_init(void)
{
    u32 val = 0;
    int ret;
    u32 regdata[14];
    const char *str;
    struct property *proper;

    /* 获取设备树中的属性数据 */
    /* 1.获取设备节点:alphaled */
    dtsled.nd = of_find_node_by_path("/alphaled");
    if(dtsled.nd == NULL) {
        printk("alphaled node can not found!\r\n");
        return -EINVAL;
    } else {
        printk("alphaled node has been found!\r\n");
    }

    /* 2.获取compatible属性内容 */
    proper = of_find_property(dtsled.nd, "compatible", NULL);
    if(proper == NULL) {
        printk("compatible property find failed\r\n");
    } else {
        printk("compatible = %s\r\n", (char *)proper->value);
    }

    /* 3.获取status属性内容 */
    ret = of_property_read_string(dtsled.nd, "status", &str);
    if(ret < 0) {
        printk("status read failed!\r\n");
    } else {
        printk("status = %s\r\n", str);
    }

    /* 4.获取reg属性内容 */
    ret = of_property_read_u32_array(dtsled.nd, "reg", regdata, 10);
    if(ret < 0) {
        printk("reg property read failed!\r\n");
    } else {
        u8 i = 0;
        printk("reg data:\r\n");
        for(i = 0; i < 10; i ++)
            printk("%#X", regdata[i]);
        printk("\r\n");
    }

#if 0
    IMX6U_CCM_CCGR1 = ioremap(regdata[0], regdata[1]);
    SW_MUX_GPIO1_IO03 = ioremap(regdata[2], regdata[3]);
    SW_PAD_GPIO1_IO03 = ioremap(regdata[4], regdata[5]);
    GPIO1_DR = ioremap(regdata[6], regdata[7]);
    GPIO1_GDIR = ioremap(regdata[8], regdata[9]);
#else
    /* index：reg 属性中要完成内存映射的段 */
    IMX6U_CCM_CCGR1 = of_iomap(dtsled.nd, 0);
    SW_MUX_GPIO1_IO03 = of_iomap(dtsled.nd, 1);
    SW_PAD_GPIO1_IO03 = of_iomap(dtsled.nd, 2);
    GPIO1_DR = of_iomap(dtsled.nd, 3);
    GPIO1_GDIR = of_iomap(dtsled.nd, 4);
#endif

    /* 2.使能GPIO1时钟 */
    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3 << 26);
    val |= (3 << 26);
    writel(val, IMX6U_CCM_CCGR1);
    /* 3.配置复用功能 */
    writel(5, SW_MUX_GPIO1_IO03);
    /* 4.配置电气属性 */
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    val = readl(GPIO1_GDIR);
    val &= ~(1 << 3);           //清除以前设置
    val |= (1 << 3);            //设置为输出模式
    writel(val, GPIO1_GDIR);

    val = readl(GPIO1_DR);
    val |= (1 << 3);            //默认关闭led
    writel(val, GPIO1_DR);

    /* 注册字符设备驱动 */
    /* 1.创建设备号 */
    if (dtsled.major) {
        dtsled.devid = MKDEV(dtsled.major, 0);
        register_chrdev_region(dtsled.devid, dtsled_CNT, dtsled_NAME);
    } else {
        alloc_chrdev_region(&dtsled.devid, 0, dtsled_CNT, dtsled_NAME);
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }
    printk("dtsled major=%d, minor=%d\r\n", dtsled.major, dtsled.minor);

    /* 2.初始化cdev */
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev, &dtsled_fops);

    /* 3.添加一个cdev */
    cdev_add(&dtsled.cdev, dtsled.devid, dtsled_CNT);

    /* 4.创建类 */
    dtsled.class = class_create(THIS_MODULE, dtsled_NAME);
    if(IS_ERR(dtsled.class)) {
        return PTR_ERR(dtsled.class);
    }

    /* 5.创建设备 */
    dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, dtsled_NAME);
    if (IS_ERR(dtsled.device)) {
        return PTR_ERR(dtsled.device);
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
    device_destroy(dtsled.class, dtsled.devid);
    /* 2.删除设备类:在/sys/class/下删除设备类目录 */
    class_destroy(dtsled.class);
    /* 3.在内核中删除cdev设备 */
    cdev_del(&dtsled.cdev);
    /* 4.取消注册cdev结构体 */
    unregister_chrdev_region(dtsled.devid, dtsled_CNT);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liaoyuan");