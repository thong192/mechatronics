#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/byteorder.h>

#define DEVICE_NAME "bmp180"
#define CLASS_NAME "bmp180_class"

#define BMP180_CHIP_ID_REG          0xD0
#define BMP180_CHIP_ID              0x55
#define BMP180_CTRL_REG             0xF4
#define BMP180_TEMP_MEASURE         0x2E
#define BMP180_PRESSURE_MEASURE     0x34
#define BMP180_OUT_MSB              0xF6

#define BMP180_IOCTL_MAGIC 'b'
#define BMP180_IOCTL_READ_TEMP     _IOR(BMP180_IOCTL_MAGIC, 1, int)
#define BMP180_IOCTL_READ_PRESSURE _IOR(BMP180_IOCTL_MAGIC, 2, int)

static struct i2c_client *bmp180_client;
static struct class *bmp180_class = NULL;
static struct device *bmp180_device = NULL;
static int major_number;

// --- Calibration data ---
struct bmp180_calib_data {
    s16 AC1, AC2, AC3;
    u16 AC4, AC5, AC6;
    s16 B1, B2;
    s16 MB, MC, MD;
};

static struct bmp180_calib_data bmp180_calib;

static int read_calib_data(struct i2c_client *client)
{
    struct bmp180_calib_data *c = &bmp180_calib;
    int ret;

    if ((ret = i2c_smbus_read_word_data(client, 0xAA)) < 0) return ret;
    c->AC1 = (s16)be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xAC)) < 0) return ret;
    c->AC2 = (s16)be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xAE)) < 0) return ret;
    c->AC3 = (s16)be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xB0)) < 0) return ret;
    c->AC4 = be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xB2)) < 0) return ret;
    c->AC5 = be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xB4)) < 0) return ret;
    c->AC6 = be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xB6)) < 0) return ret;
    c->B1 = (s16)be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xB8)) < 0) return ret;
    c->B2 = (s16)be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xBA)) < 0) return ret;
    c->MB = (s16)be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xBC)) < 0) return ret;
    c->MC = (s16)be16_to_cpu(ret);

    if ((ret = i2c_smbus_read_word_data(client, 0xBE)) < 0) return ret;
    c->MD = (s16)be16_to_cpu(ret);

    return 0;
}

static int bmp180_read_temp_raw(void)
{
    int ret;

    if (!bmp180_client)
        return -ENODEV;

    ret = i2c_smbus_write_byte_data(bmp180_client, BMP180_CTRL_REG, BMP180_TEMP_MEASURE);
    if (ret < 0)
        return ret;
    msleep(5);
    int msb = i2c_smbus_read_byte_data(bmp180_client, BMP180_OUT_MSB);
    int lsb = i2c_smbus_read_byte_data(bmp180_client, BMP180_OUT_MSB + 1);
    return (msb << 8) | lsb;
}

static int bmp180_read_pressure_raw(void)
{
    int ret;

    if (!bmp180_client) return -ENODEV;

    ret = i2c_smbus_write_byte_data(bmp180_client, BMP180_CTRL_REG, BMP180_PRESSURE_MEASURE);
    if (ret < 0) return ret;

    msleep(5);

    int msb = i2c_smbus_read_byte_data(bmp180_client, BMP180_OUT_MSB);
    int lsb = i2c_smbus_read_byte_data(bmp180_client, BMP180_OUT_MSB + 1);
    int xlsb = i2c_smbus_read_byte_data(bmp180_client, BMP180_OUT_MSB + 2);
    return ((msb << 16) | (lsb << 8) | xlsb) >> 8;
}

static int bmp180_get_temperature(void)
{
    int ut = bmp180_read_temp_raw();
    if (ut < 0) return ut;
    int x1 = ((ut - bmp180_calib.AC6) * bmp180_calib.AC5) >> 15;
    int x2 = (bmp180_calib.MC << 11) / (x1 + bmp180_calib.MD);
    int b5 = x1 + x2;
    return (b5 + 8) >> 4; // 0.1 °C
}

static int bmp180_get_pressure(void)
{
    int ut = bmp180_read_temp_raw();
    int up = bmp180_read_pressure_raw();

    int x1, x2, x3, b3, b5, b6;
    unsigned int b4, b7;
    int pressure;

    x1 = ((ut - bmp180_calib.AC6) * bmp180_calib.AC5) >> 15;
    x2 = (bmp180_calib.MC << 11) / (x1 + bmp180_calib.MD);
    b5 = x1 + x2;

    b6 = b5 - 4000;
    x1 = (bmp180_calib.B2 * ((b6 * b6) >> 12)) >> 11;
    x2 = (bmp180_calib.AC2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = ((((long)bmp180_calib.AC1 * 4 + x3)) + 2) >> 2;

    x1 = (bmp180_calib.AC3 * b6) >> 13;
    x2 = (bmp180_calib.B1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    b4 = (bmp180_calib.AC4 * (unsigned int)(x3 + 32768)) >> 15;
    b7 = ((unsigned int)(up - b3) * 50000);

    if (b7 < 0x80000000)
        pressure = (b7 << 1) / b4;
    else
        pressure = (b7 / b4) << 1;

    x1 = (pressure >> 8) * (pressure >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * pressure) >> 16;
    pressure = pressure + ((x1 + x2 + 3791) >> 4);

    return pressure; // in Pa
}

// --- ioctl handler ---
static long bmp180_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int val = 0;

    switch (cmd) {
        case BMP180_IOCTL_READ_TEMP:
            val = bmp180_get_temperature(); // 0.1 °C
            break;
        case BMP180_IOCTL_READ_PRESSURE:
            val = bmp180_get_pressure(); // Pa
            break;
        default:
            return -EINVAL;
    }

    if (copy_to_user((int __user *)arg, &val, sizeof(val)))
        return -EFAULT;

    return 0;
}

static int bmp180_open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "BMP180 device opened\n");
    return 0;
}

static int bmp180_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "BMP180 device closed\n");
    return 0;
}

// --- file ops ---
static struct file_operations bmp180_fops = {
    .open = bmp180_open,
    .unlocked_ioctl = bmp180_ioctl,
    .release = bmp180_release, 
};

// --- I2C driver ---
static int bmp180_probe(struct i2c_client *client)
{
    int chip_id;

    bmp180_client = client;
    chip_id = i2c_smbus_read_byte_data(client, BMP180_CHIP_ID_REG);
    if (chip_id != BMP180_CHIP_ID) {
        dev_err(&client->dev, "BMP180 not found\n");
        return -ENODEV;
    }

    read_calib_data(client);

    major_number = register_chrdev(0, DEVICE_NAME, &bmp180_fops);
    if (major_number < 0) {
        dev_err(&client->dev, "Failed to register a major number\n");
        return major_number;
    }

    bmp180_class = class_create(CLASS_NAME);
    if (IS_ERR(bmp180_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        dev_err(&client->dev, "Failed to register device class\n");
        return PTR_ERR(bmp180_class);
    }

    bmp180_device = device_create(bmp180_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(bmp180_device)) {
        class_destroy(bmp180_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        dev_err(&client->dev, "Failed to create the device\n");
        return PTR_ERR(bmp180_device);
    }

    dev_info(&client->dev, "BMP180 driver registered with /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void bmp180_remove(struct i2c_client *client)
{
    device_destroy(bmp180_class, MKDEV(major_number, 0));
    class_unregister(bmp180_class);
    class_destroy(bmp180_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    dev_info(&client->dev, "BMP180 driver removed\n");
}

static const struct of_device_id bmp180_of_match[] = {
    { .compatible = "bosch,bmp180", },
    { }
};
MODULE_DEVICE_TABLE(of, bmp180_of_match);

static struct i2c_driver bmp180_driver = {
    .driver = {
        .name = "bmp180",
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(bmp180_of_match),
    },
    .probe = bmp180_probe,
    .remove = bmp180_remove,
};

static int __init bmp180_init(void)
{
    printk(KERN_INFO "Initializing BMP180 driver\n");
    return i2c_add_driver(&bmp180_driver);
}

static void __exit bmp180_exit(void)
{
    printk(KERN_INFO "Exiting BMP180 driver\n");
    i2c_del_driver(&bmp180_driver);
}

module_init(bmp180_init);
module_exit(bmp180_exit);

// module_i2c_driver(bmp180_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raspi");
MODULE_DESCRIPTION("BMP180 char device driver with ioctl support");
