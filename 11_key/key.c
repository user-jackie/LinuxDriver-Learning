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

#define KEY_CNT     1
#define KEY_NAME    "key"

#define KEY0VALUE   0XF0
#define INVAKEY     0x00

struct key_dev {
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
    int key_gpio;
    atomic_t keyvalue;
};

struct key_dev key;

static int keyio_init(void)
{
    key.nd = of_find_node_by_path("/key");
    if(key.nd == NULL) {
        printk("can't find node!\r\n");
        return -EINVAL;
    }
    printk("node has been found!\r\n");

    /* 对于1个gpio的外设来说,index=0 */
    key.key_gpio = of_get_named_gpio(key.nd, "key-gpio", 0);
    if(key.key_gpio < 0) {
        printk("can't get key-gpio!\r\n");
        return -EINVAL;
    }
    printk("key-gpio num = %d\r\n", key.key_gpio);

    gpio_request(key.key_gpio, "key0");
    gpio_direction_input(key.key_gpio);

    return 0;
}

static int key_open(struct inode *inode, struct file *filp)
{
    int ret;

    /* private_data是地址型数据 */
    filp->private_data = &key;

    ret = keyio_init();
    if(ret < 0) {
        return ret;
    }

    return 0;
}

static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    unsigned char value;
    struct key_dev *dev = filp->private_data;

    /* 从dev.key_gpio这个id号中读取值 */
    if(gpio_get_value(dev->key_gpio) == 0) {
        while(!gpio_get_value(dev->key_gpio));
        atomic_set(&dev->keyvalue, KEY0VALUE);
    } else {
        atomic_set(&dev->keyvalue, INVAKEY);
    }

    value = atomic_read(&dev->keyvalue);
    ret = copy_to_user(buf, &value, sizeof(value));

    return ret;
}

/* 设备操作函数 */
static struct file_operations key_fops = {
    .owner = THIS_MODULE,
    .open = key_open,
    .read = key_read,
};

static int __init mykey_init(void)
{
    /* 初始化原子变量 */
    atomic_set(&key.keyvalue, INVAKEY);

    /* 注册字符设备驱动 */
    if(key.major) {
        key.devid = MKDEV(key.major, 0);
        register_chrdev_region(key.devid, KEY_CNT, KEY_NAME);
    } else {
        alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);
        key.major = MAJOR(key.devid);
        key.minor = MINOR(key.devid);
    }
    printk("major=%d, minor=%d\r\n", key.major, key.minor);

    key.cdev.owner = THIS_MODULE;
    cdev_init(&key.cdev, &key_fops);

    cdev_add(&key.cdev, key.devid, KEY_CNT);

    key.class = class_create(THIS_MODULE, KEY_NAME);
    if(IS_ERR(key.class)) {
        return PTR_ERR(key.class);
    }

    key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
    if(IS_ERR(key.device)) {
        return PTR_ERR(key.device);
    }

    return 0;
}

static void __exit mykey_exit(void)
{
    gpio_free(key.key_gpio);
    device_destroy(key.class, key.devid);
    class_destroy(key.class);
    cdev_del(&key.cdev);
    unregister_chrdev_region(key.devid, KEY_CNT);
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liaoyuan");

