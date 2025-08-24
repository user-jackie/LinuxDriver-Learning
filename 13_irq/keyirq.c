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
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define KEYIRQ_CNT      1
#define KEYIRQ_NAME     "keyirq"
#define KEY0VALUE       0X01
#define INVAKEY         0XFF
#define KEY_NUM         1           /* 按键数量 */

/* 中断IO描述结构体 */
struct irq_keydesc {
    int gpio;
    int irqnum;
    unsigned char value;
    char name[10];
    irqreturn_t (*handler) (int, void *);       /* 中断服务函数 */
};

struct keyirq_dev {
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
    atomic_t keyvalue;
    atomic_t releasekey;
    struct timer_list timer;
    struct irq_keydesc irqkeydesc[KEY_NUM];     /*按键描述数组*/
    unsigned char curkeynum;                    /* 当前按下的按键号 */
};

struct keyirq_dev keyirq;

static irqreturn_t key0_handler(int irq, void *dev_id)
{
    struct keyirq_dev *dev = (struct keyirq_dev *)dev_id;

    dev->curkeynum = 0;
    dev->timer.data = (volatile long)dev_id;    /* 传递给定时器中断的结构体首地址 */
    /* 按键按下后10ms触发定时器中断 */
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
    return IRQ_RETVAL(IRQ_HANDLED);
}

/* 定时器中断处理函数不加static，可供外部调用 */
void timer_function(unsigned long arg)
{
    unsigned char value;
    unsigned char num;
    struct irq_keydesc *keydesc;
    struct keyirq_dev *dev = (struct keyirq_dev *)arg;

    num = dev->curkeynum;
    keydesc = &dev->irqkeydesc[num];        /* 得到当前按下按键对应的按键数组 */
    value = gpio_get_value(keydesc->gpio);

    if(value == 0) {        /* 按键按下 */
        atomic_set(&dev->keyvalue, keydesc->value);     /* 设置dev中的按键值 */
    } else {                /* 按键松开 */
        atomic_set(&dev->keyvalue, 0x80 | keydesc->value);
        atomic_set(&dev->releasekey, 1);        /* 标记按键松开 */
    }
}

static int keyio_init(void)
{
    unsigned char i = 0;
    int ret = 0;

    keyirq.nd = of_find_node_by_path("/key");
    if(keyirq.nd == NULL) {
        printk("can't find node!\r\n");
        return -EINVAL;
    }

    /* 提取GPIO */
    for (i = 0; i < KEY_NUM; i ++) {
        // ??
        keyirq.irqkeydesc[i].gpio = of_get_named_gpio(keyirq.nd, "key-gpio", i);
        if(keyirq.irqkeydesc[i].gpio < 0) {
            printk("can't find key%d!\r\n", i);
            return -EINVAL;
        }
    }

    /* 初始化key所用的IO，并设置成中断模式 */
    for (i = 0; i < KEY_NUM; i ++) {
        memset(keyirq.irqkeydesc[i].name, 0, sizeof(keyirq.irqkeydesc[i].name));
        sprintf(keyirq.irqkeydesc[i].name, "KEY%d", i);
        gpio_request(keyirq.irqkeydesc[i].gpio, keyirq.irqkeydesc[i].name);
        gpio_direction_input(keyirq.irqkeydesc[i].gpio);
        // ??
        keyirq.irqkeydesc[i].irqnum = irq_of_parse_and_map(keyirq.nd, i);
#if 0
        keyirq.irqkeydesc[i].irqnum = gpio_to_irq(keyirq.irqkeydesc[i].gpio);
#endif
        printk("key%d:gpio=%d, irqnum=%d\r\n", i, keyirq.irqkeydesc[i].gpio, keyirq.irqkeydesc[i].irqnum);
    }

    keyirq.irqkeydesc[0].handler = key0_handler;
    keyirq.irqkeydesc[0].value = KEY0VALUE;

    for(i = 0; i < KEY_NUM; i ++) {
        ret = request_irq(  keyirq.irqkeydesc[i].irqnum, 
                            keyirq.irqkeydesc[i].handler, 
                            IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
                            keyirq.irqkeydesc[i].name, 
                            &keyirq);
        if(ret < 0) {
            printk("irq %d request failed!\r\n", keyirq.irqkeydesc[i].irqnum);
            return -EINVAL;
        }
    }

    init_timer(&keyirq.timer);
    keyirq.timer.function = timer_function;
    return 0;
}

static int keyirq_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &keyirq;
    return 0;
}

static ssize_t keyirq_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret = 0;
    unsigned char keyvalue = 0;
    unsigned char releasekey = 0;
    struct keyirq_dev *dev = (struct keyirq_dev *)filp->private_data;
    keyvalue = atomic_read(&dev->keyvalue);
    releasekey = atomic_read(&dev->releasekey);

    if(releasekey) {
        if(keyvalue & 0x80) {
            keyvalue &= ~0x80;
            ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
        } else {
            // ??
            goto data_error;
        }
        atomic_set(&dev->releasekey, 0);
    } else {
        goto data_error;
    }
    return 0;

data_error:
    return -EINVAL;
}

static struct file_operations keyirq_fops = {
    .owner = THIS_MODULE,
    .open = keyirq_open,
    .read = keyirq_read,
};

static int __init keyirq_init(void)
{
    if(keyirq.major) {
        keyirq.devid = MKDEV(keyirq.devid, 0);
        register_chrdev_region(keyirq.devid, 0, KEYIRQ_NAME);
    } else {
        alloc_chrdev_region(&keyirq.devid, 0, KEYIRQ_CNT, KEYIRQ_NAME);
        keyirq.major = MAJOR(keyirq.devid);
        keyirq.minor = MINOR(keyirq.devid);
    }
    printk("major=%d, minor=%d\r\n", keyirq.major, keyirq.minor);

    keyirq.cdev.owner = THIS_MODULE;
    cdev_init(&keyirq.cdev, &keyirq_fops);

    cdev_add(&keyirq.cdev, keyirq.devid, KEYIRQ_CNT);

    keyirq.class = class_create(THIS_MODULE, KEYIRQ_NAME);
    if(IS_ERR(keyirq.class)) {
        return PTR_ERR(keyirq.class);
    }
    keyirq.device = device_create(keyirq.class, NULL, keyirq.devid, NULL, KEYIRQ_NAME);
    if(IS_ERR(keyirq.device)) {
        return PTR_ERR(keyirq.device);
    }

    /* 初始化按键 */
    atomic_set(&keyirq.keyvalue, INVAKEY);
    atomic_set(&keyirq.releasekey, 0);
    keyio_init();
    return 0;
}

static void __exit keyirq_exit(void)
{
    unsigned int i;
    del_timer_sync(&keyirq.timer);

    for(i = 0; i < KEY_NUM; i ++) {
        free_irq(keyirq.irqkeydesc[i].irqnum, &keyirq);
        gpio_free(keyirq.irqkeydesc[i].gpio);
    }
    device_destroy(keyirq.class, keyirq.devid);
    class_destroy(keyirq.class);
    cdev_del(&keyirq.cdev);
    unregister_chrdev_region(keyirq.devid, KEYIRQ_CNT);
}

module_init(keyirq_init);
module_exit(keyirq_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liaoyuan");