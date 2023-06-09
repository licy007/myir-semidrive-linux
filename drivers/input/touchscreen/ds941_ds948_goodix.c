/*
 *  Driver for Goodix Touchscreens
 *
 *  Copyright (c) 2014 Red Hat Inc.
 *  Copyright (c) 2015 K. Merker <merker@debian.org>
 *
 *  This code is based on gt9xx.c authored by andrew@goodix.com:
 *
 *  2010 - 2012 Goodix Technology.
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2 of the License.
 */

#include <linux/kernel.h>
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <asm/unaligned.h>
#include <linux/kthread.h>

struct goodix_ts_data {
    struct i2c_client *client;
    struct input_dev *input_dev;
    int abs_x_max;
    int abs_y_max;
    bool swapped_x_y;
    bool inverted_x;
    bool inverted_y;
    unsigned int max_touch_num;
    unsigned int int_trigger_type;
    int cfg_len;
    struct gpio_desc *gpiod_int;
    struct gpio_desc *gpiod_rst;
    u16 id;
    u16 version;
    u32 dev_type;   /* 0: main device, 1: aux device */
    u32 port_type;   /* 0: primary, 1: second, 2: doul */
    u8 addr_ds941;
    u8 addr_ds948;
    int irq_channel;
    int reset_channel;
    u32 thread_probe;
    const char *cfg_name;
    struct completion firmware_loading_complete;
    unsigned long irq_flags;
};

#define GOODIX_GPIO_INT_NAME        "irq"
#define GOODIX_GPIO_RST_NAME        "reset"

#define GOODIX_MAX_HEIGHT       4096
#define GOODIX_MAX_WIDTH        4096
#define GOODIX_INT_TRIGGER      1
#define GOODIX_CONTACT_SIZE     8
#define GOODIX_MAX_CONTACTS     10

#define GOODIX_CONFIG_MAX_LENGTH    240
#define GOODIX_CONFIG_911_LENGTH    186
#define GOODIX_CONFIG_967_LENGTH    228

/* Register defines */
#define GOODIX_REG_COMMAND      0x8040
#define GOODIX_CMD_SCREEN_OFF       0x05

#define GOODIX_READ_COOR_ADDR       0x814E
#define GOODIX_REG_CONFIG_DATA      0x8047
#define GOODIX_REG_ID           0x8140

#define GOODIX_BUFFER_STATUS_READY  BIT(7)
#define GOODIX_BUFFER_STATUS_TIMEOUT    20

#define RESOLUTION_LOC      1
#define MAX_CONTACTS_LOC    5
#define TRIGGER_LOC     6

#define TP_USE_THREAD 0

#define USE_RST_BYPASS 0

static const unsigned long goodix_irq_flags[] = {
    IRQ_TYPE_EDGE_RISING,
    IRQ_TYPE_EDGE_FALLING,
    IRQ_TYPE_LEVEL_LOW,
    IRQ_TYPE_LEVEL_HIGH,
};

/*
 * Those tablets have their coordinates origin at the bottom right
 * of the tablet, as if rotated 180 degrees
 */
static const struct dmi_system_id rotated_screen[] = {
#if defined(CONFIG_DMI) && defined(CONFIG_X86)
    {
        .ident = "WinBook TW100",
        .matches = {
            DMI_MATCH(DMI_SYS_VENDOR, "WinBook"),
            DMI_MATCH(DMI_PRODUCT_NAME, "TW100")
        }
    },
    {
        .ident = "WinBook TW700",
        .matches = {
            DMI_MATCH(DMI_SYS_VENDOR, "WinBook"),
            DMI_MATCH(DMI_PRODUCT_NAME, "TW700")
        },
    },
#endif
    {}
};

/**
 * goodix_i2c_read - read data from a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to read from.
 * @buf: raw write data buffer.
 * @len: length of the buffer to write
 */
static int goodix_i2c_read(struct i2c_client *client,
                           u16 reg, u8 *buf, int len)
{
    struct i2c_msg msgs[2];
    u16 wbuf = cpu_to_be16(reg);
    int ret;

    msgs[0].flags = 0;
    msgs[0].addr  = client->addr;
    msgs[0].len   = 2;
    msgs[0].buf   = (u8 *)&wbuf;

    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = client->addr;
    msgs[1].len   = len;
    msgs[1].buf   = buf;

    ret = i2c_transfer(client->adapter, msgs, 2);
    return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}

static int du90ub941_i2c_read(struct goodix_ts_data *ts,
                              u8 reg, u8 *buf, int len)
{
    struct i2c_msg msgs[2];
    int ret;

    msgs[0].flags = 0;
    msgs[0].addr  = ts->addr_ds941;
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = ts->addr_ds941;
    msgs[1].len   = len;
    msgs[1].buf   = buf;

    ret = i2c_transfer(ts->client->adapter, msgs, 2);
    return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}

static int du90ub941_i2c_write(struct goodix_ts_data *ts,
                               u8 reg, u8 *buf, int len)
{
    u8 *addr_buf;
    struct i2c_msg msg;
    int ret;

    addr_buf = kmalloc(len + 1, GFP_KERNEL);

    if (!addr_buf)
        return -ENOMEM;

    addr_buf[0] = reg;
    memcpy(&addr_buf[1], buf, len);

    msg.flags = 0;
    msg.addr = ts->addr_ds941;
    msg.buf = addr_buf;
    msg.len = len + 1;

    ret = i2c_transfer(ts->client->adapter, &msg, 1);
    kfree(addr_buf);
    return ret < 0 ? ret : (ret != 1 ? -EIO : 0);
}

static int du90ub948_i2c_read(struct goodix_ts_data *ts,
                              u8 reg, u8 *buf, int len)
{
    struct i2c_msg msgs[2];
    int ret;

    msgs[0].flags = 0;
    msgs[0].addr  = ts->addr_ds948;
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = ts->addr_ds948;
    msgs[1].len   = len;
    msgs[1].buf   = buf;

    ret = i2c_transfer(ts->client->adapter, msgs, 2);
    return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}

static int du90ub948_i2c_write(struct goodix_ts_data *ts, u8 reg,
                               const u8 *buf,
                               unsigned len)
{
    u8 *addr_buf;
    struct i2c_msg msg;
    int ret;

    addr_buf = kmalloc(len + 1, GFP_KERNEL);

    if (!addr_buf)
        return -ENOMEM;

    addr_buf[0] = reg;
    memcpy(&addr_buf[1], buf, len);

    msg.flags = 0;
    msg.addr = ts->addr_ds948;
    msg.buf = addr_buf;
    msg.len = len + 1;

    ret = i2c_transfer(ts->client->adapter, &msg, 1);
    kfree(addr_buf);
    return ret < 0 ? ret : (ret != 1 ? -EIO : 0);
}

#if USE_RST_BYPASS
static int du90ub941_948_gpio2_output(struct goodix_ts_data *ts)
{
    u8 dreg, dval;

    dreg = 0x0e;
    dval = 0;
    du90ub941_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "941 before reg=0x%x, val=0x%x\n", dreg, dval);
    dval &= 0x0f;
    dval |= 0x30;
    du90ub941_i2c_write(ts, dreg, &dval, 1);
    dval = 0;
    du90ub941_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "941 after reg=0x%x, val=0x%x\n", dreg, dval);

    dreg = 0x1e;
    dval = 0;
    du90ub948_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "948 before reg=0x%x, val=0x%x\n", dreg, dval);
    dval &= 0x0f;
    dval |= 0x50;
    du90ub948_i2c_write(ts, dreg, &dval, 1);
    dval = 0;
    du90ub948_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "948 after reg=0x%x, val=0x%x\n", dreg, dval);

    return 0;
}
#endif

#if 0
static int du90ub941_948_gpio3_output(struct goodix_ts_data *ts)
{
    u8 dreg, dval;

    dreg = 0x0f;
    dval = 0;
    du90ub941_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "941 before out reg=0x%x, val=0x%x\n", dreg,
            dval);
    dval &= 0xf0;
    dval |= 0x03;
    du90ub941_i2c_write(ts, dreg, &dval, 1);
    dval = 0;
    du90ub941_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "941 after out reg=0x%x, val=0x%x\n", dreg,
            dval);

    dreg = 0x1f;
    dval = 0;
    du90ub948_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "948 before out reg=0x%x, val=0x%x\n", dreg,
            dval);
    dval &= 0xf0;
    dval |= 0x05;
    du90ub948_i2c_write(ts, dreg, &dval, 1);
    dval = 0;
    du90ub948_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "948 after out reg=0x%x, val=0x%x\n", dreg,
            dval);

    return 0;
}

static int du90ub941_948_gpio3_input(struct goodix_ts_data *ts)
{
    u8 dreg, dval;

    dreg = 0x0f;
    dval = 0;
    du90ub941_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "941 in reg=0x%x, val=0x%x\n", dreg, dval);
    dval &= 0xf0;
    dval |= 0x05;
    du90ub941_i2c_write(ts, dreg, &dval, 1);

    dreg = 0x1f;
    dval = 0;
    du90ub948_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "948 in reg=0x%x, val=0x%x\n", dreg, dval);
    dval &= 0xf0;
    dval |= 0x03;
    du90ub948_i2c_write(ts, dreg, &dval, 1);

    return 0;
}
#endif

static int du90ub941_948_gpio_output(struct goodix_ts_data *ts, int gpio)
{
    u8 dreg, dval;

    switch (gpio) {
        case 0:
            dreg = 0x0d;
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 before out reg=0x%x, val=0x%x\n", dreg,
                    dval);
            dval &= 0xf0;
            dval |= 0x03;
            du90ub941_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 after out reg=0x%x, val=0x%x\n", dreg,
                    dval);

            dreg = 0x1d;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 before out reg=0x%x, val=0x%x\n", dreg,
                    dval);
            dval &= 0xf0;
            dval |= 0x05;
            du90ub948_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 after out reg=0x%x, val=0x%x\n", dreg,
                    dval);
            break;

        case 1:
            dreg = 0x0e;
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 before out reg=0x%x, val=0x%x\n", dreg,
                    dval);
            dval &= 0xf0;
            dval |= 0x03;
            du90ub941_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 after out reg=0x%x, val=0x%x\n", dreg,
                    dval);

            dreg = 0x1e;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 before out reg=0x%x, val=0x%x\n", dreg,
                    dval);
            dval &= 0xf0;
            dval |= 0x05;
            du90ub948_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 after out reg=0x%x, val=0x%x\n", dreg,
                    dval);
            break;

        case 2:
            dreg = 0x0e;
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 before reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0x0f;
            dval |= 0x30;
            du90ub941_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 after reg=0x%x, val=0x%x\n", dreg, dval);

            dreg = 0x1e;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 before reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0x0f;
            dval |= 0x50;
            du90ub948_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 after reg=0x%x, val=0x%x\n", dreg, dval);
            break;

        case 3:

            dreg = 0x0f;
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 before out reg=0x%x, val=0x%x\n", dreg,
                    dval);
            dval &= 0xf0;
            dval |= 0x03;
            du90ub941_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 after out reg=0x%x, val=0x%x\n", dreg,
                    dval);

            dreg = 0x1f;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 before out reg=0x%x, val=0x%x\n", dreg,
                    dval);
            dval &= 0xf0;
            dval |= 0x05;
            du90ub948_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 after out reg=0x%x, val=0x%x\n", dreg,
                    dval);
            break;

        default:
            dev_err(&ts->client->dev, "941 948 gpio is error: %d\n", gpio);
            break;
    }

    return 0;
}

static int du90ub948_gpio_output_val(struct goodix_ts_data *ts, int gpio,
                                     int val)
{
    u8 dreg, dval;

    switch (gpio) {
        case 0:
            dreg = 0x1d;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 before reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0xf0;

            if (val == 1)
                dval |= 0x09;
            else
                dval |= 0x01;

            du90ub948_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 after reg=0x%x, val=0x%x\n", dreg, dval);
            break;

        case 1:
            dreg = 0x1e;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 before reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0xf0;

            if (val == 1)
                dval |= 0x09;
            else
                dval |= 0x01;

            du90ub948_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 after reg=0x%x, val=0x%x\n", dreg, dval);
            break;

        case 2:
            dreg = 0x1e;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 before reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0x0f;

            if (val == 1)
                dval |= 0x90;
            else
                dval |= 0x10;

            du90ub948_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 after reg=0x%x, val=0x%x\n", dreg, dval);
            break;

        case 3:
            dreg = 0x1f;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 before reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0xf0;

            if (val == 1)
                dval |= 0x09;
            else
                dval |= 0x01;

            du90ub948_i2c_write(ts, dreg, &dval, 1);
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 after reg=0x%x, val=0x%x\n", dreg, dval);
            break;

        default:
            dev_err(&ts->client->dev, "948 gpio is error: %d\n", gpio);
            break;
    }

    return 0;
}
static int du90ub941_948_gpio_input(struct goodix_ts_data *ts, int gpio)
{
    u8 dreg, dval;

    switch (gpio) {
        case 0:
            dreg = 0x0d;
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 in reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0xf0;
            dval |= 0x05;
            du90ub941_i2c_write(ts, dreg, &dval, 1);

            dreg = 0x1d;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 in reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0xf0;
            dval |= 0x03;
            du90ub948_i2c_write(ts, dreg, &dval, 1);
            break;

        case 1:
            dreg = 0x0e;
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 in reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0xf0;
            dval |= 0x05;
            du90ub941_i2c_write(ts, dreg, &dval, 1);

            dreg = 0x1e;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 in reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0xf0;
            dval |= 0x03;
            du90ub948_i2c_write(ts, dreg, &dval, 1);
            break;

        case 2:
            dreg = 0x0e;
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 in reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0x0f;
            dval |= 0x50;
            du90ub941_i2c_write(ts, dreg, &dval, 1);

            dreg = 0x1e;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 in reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0x0f;
            dval |= 0x30;
            du90ub948_i2c_write(ts, dreg, &dval, 1);

            break;

        case 3:
            dreg = 0x0f;
            dval = 0;
            du90ub941_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "941 in reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0xf0;
            dval |= 0x05;
            du90ub941_i2c_write(ts, dreg, &dval, 1);

            dreg = 0x1f;
            dval = 0;
            du90ub948_i2c_read(ts, dreg, &dval, 1);
            dev_err(&ts->client->dev, "948 in reg=0x%x, val=0x%x\n", dreg, dval);
            dval &= 0xf0;
            dval |= 0x03;
            du90ub948_i2c_write(ts, dreg, &dval, 1);
            break;

        default:
            dev_err(&ts->client->dev, "941 948 gpio is error: %d\n", gpio);
            break;
    }

    return 0;
}

static int du90ub941_enable_port(struct goodix_ts_data *ts,
                                 int port_index)
{
    u8 dreg, dval;
    dreg = 0x1e;
    dval = 0;
    du90ub941_i2c_read(ts, dreg, &dval, 1);
    dev_err(&ts->client->dev, "941[4] reg=0x%x, val=0x%x\n", dreg, dval);

    if (port_index == 0)
        dval = 0x1;
    else if (port_index == 1)
        dval = 0x4;
    else
        return 0;

    du90ub941_i2c_write(ts, dreg, &dval, 1);

    return 0;
}


/**
 * goodix_i2c_write - write data to a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to write to.
 * @buf: raw data buffer to write.
 * @len: length of the buffer to write
 */
static int goodix_i2c_write(struct i2c_client *client, u16 reg,
                            const u8 *buf,
                            unsigned len)
{
    u8 *addr_buf;
    struct i2c_msg msg;
    int ret;

    addr_buf = kmalloc(len + 2, GFP_KERNEL);

    if (!addr_buf)
        return -ENOMEM;

    addr_buf[0] = reg >> 8;
    addr_buf[1] = reg & 0xFF;
    memcpy(&addr_buf[2], buf, len);

    msg.flags = 0;
    msg.addr = client->addr;
    msg.buf = addr_buf;
    msg.len = len + 2;

    ret = i2c_transfer(client->adapter, &msg, 1);
    kfree(addr_buf);
    return ret < 0 ? ret : (ret != 1 ? -EIO : 0);
}

static int goodix_i2c_write_u8(struct i2c_client *client, u16 reg,
                               u8 value)
{
    return goodix_i2c_write(client, reg, &value, sizeof(value));
}

static int goodix_get_cfg_len(u16 id)
{
    switch (id) {
        case 911:
        case 9271:
        case 9110:
        case 927:
        case 928:
            return GOODIX_CONFIG_911_LENGTH;

        case 912:
        case 967:
            return GOODIX_CONFIG_967_LENGTH;

        default:
            return GOODIX_CONFIG_MAX_LENGTH;
    }
}

static int goodix_ts_read_input_report(struct goodix_ts_data *ts,
                                       u8 *data)
{
    unsigned long max_timeout;
    int touch_num;
    int error;

    /*
     * The 'buffer status' bit, which indicates that the data is valid, is
     * not set as soon as the interrupt is raised, but slightly after.
     * This takes around 10 ms to happen, so we poll for 20 ms.
     */
    max_timeout = jiffies + msecs_to_jiffies(GOODIX_BUFFER_STATUS_TIMEOUT);

    do {
        error = goodix_i2c_read(ts->client, GOODIX_READ_COOR_ADDR,
                                data, GOODIX_CONTACT_SIZE + 1);

        if (error) {
            dev_err(&ts->client->dev, "I2C transfer error: %d\n",
                    error);
            return error;
        }

        if (data[0] & GOODIX_BUFFER_STATUS_READY) {
            touch_num = data[0] & 0x0f;

            if (touch_num > ts->max_touch_num)
                return -EPROTO;

            if (touch_num > 1) {
                data += 1 + GOODIX_CONTACT_SIZE;
                error = goodix_i2c_read(ts->client,
                                        GOODIX_READ_COOR_ADDR +
                                        1 + GOODIX_CONTACT_SIZE,
                                        data,
                                        GOODIX_CONTACT_SIZE *
                                        (touch_num - 1));

                if (error)
                    return error;
            }

            return touch_num;
        }

        usleep_range(1000, 2000); /* Poll every 1 - 2 ms */
    }
    while (time_before(jiffies, max_timeout));

    /*
     * The Goodix panel will send spurious interrupts after a
     * 'finger up' event, which will always cause a timeout.
     */
    return 0;
}

static void goodix_ts_report_touch(struct goodix_ts_data *ts,
                                   u8 *coor_data)
{
    int id = coor_data[0] & 0x0F;
    int input_x = get_unaligned_le16(&coor_data[1]);
    int input_y = get_unaligned_le16(&coor_data[3]);
    int input_w = get_unaligned_le16(&coor_data[5]);

    /* Inversions have to happen before axis swapping */
    if (ts->inverted_x)
        input_x = ts->abs_x_max - input_x;

    if (ts->inverted_y)
        input_y = ts->abs_y_max - input_y;

    if (ts->swapped_x_y)
        swap(input_x, input_y);

    input_mt_slot(ts->input_dev, id);
    input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
    input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
    input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, input_w);
    //printk("point: (%d, %d)\n", input_x, input_y);
    //dev_info(&ts->client->dev, "point: (%d, %d)\n", input_x, input_y);
}

/**
 * goodix_process_events - Process incoming events
 *
 * @ts: our goodix_ts_data pointer
 *
 * Called when the IRQ is triggered. Read the current device state, and push
 * the input events to the user space.
 */
static void goodix_process_events(struct goodix_ts_data *ts)
{
    u8  point_data[1 + GOODIX_CONTACT_SIZE * GOODIX_MAX_CONTACTS];
    int touch_num;
    int i;

    touch_num = goodix_ts_read_input_report(ts, point_data);

    if ((touch_num < 0) || (point_data[0] == 0))
        return;

    /*
     * Bit 4 of the first byte reports the status of the capacitive
     * Windows/Home button.
     */
    input_report_key(ts->input_dev, KEY_LEFTMETA, point_data[0] & BIT(4));

    for (i = 0; i < touch_num; i++)
        goodix_ts_report_touch(ts,
                               &point_data[1 + GOODIX_CONTACT_SIZE * i]);

    input_mt_sync_frame(ts->input_dev);
    input_sync(ts->input_dev);
}

/**
 * goodix_ts_irq_handler - The IRQ handler
 *
 * @irq: interrupt number.
 * @dev_id: private data pointer.
 */
static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
    struct goodix_ts_data *ts = dev_id;
    //dev_info(&ts->client->dev, "%s\n", __func__);
    goodix_process_events(ts);

    if (goodix_i2c_write_u8(ts->client, GOODIX_READ_COOR_ADDR, 0) < 0)
        dev_err(&ts->client->dev, "I2C write end_cmd error\n");

    return IRQ_HANDLED;
}

static void goodix_free_irq(struct goodix_ts_data *ts)
{
    devm_free_irq(&ts->client->dev, ts->client->irq, ts);
}

static int goodix_request_irq(struct goodix_ts_data *ts)
{
    return devm_request_threaded_irq(&ts->client->dev, ts->client->irq,
                                     NULL, goodix_ts_irq_handler,
                                     ts->irq_flags, ts->client->name, ts);
}

/**
 * goodix_check_cfg - Checks if config fw is valid
 *
 * @ts: goodix_ts_data pointer
 * @cfg: firmware config data
 */
static int goodix_check_cfg(struct goodix_ts_data *ts,
                            const struct firmware *cfg)
{
    int i, raw_cfg_len;
    u8 check_sum = 0;

    if (cfg->size > GOODIX_CONFIG_MAX_LENGTH) {
        dev_err(&ts->client->dev,
                "The length of the config fw is not correct");
        return -EINVAL;
    }

    raw_cfg_len = cfg->size - 2;

    for (i = 0; i < raw_cfg_len; i++)
        check_sum += cfg->data[i];

    check_sum = (~check_sum) + 1;

    if (check_sum != cfg->data[raw_cfg_len]) {
        dev_err(&ts->client->dev,
                "The checksum of the config fw is not correct");
        return -EINVAL;
    }

    if (cfg->data[raw_cfg_len + 1] != 1) {
        dev_err(&ts->client->dev,
                "Config fw must have Config_Fresh register set");
        return -EINVAL;
    }

    return 0;
}

/**
 * goodix_send_cfg - Write fw config to device
 *
 * @ts: goodix_ts_data pointer
 * @cfg: config firmware to write to device
 */
static int goodix_send_cfg(struct goodix_ts_data *ts,
                           const struct firmware *cfg)
{
    int error;

    error = goodix_check_cfg(ts, cfg);

    if (error)
        return error;

    error = goodix_i2c_write(ts->client, GOODIX_REG_CONFIG_DATA, cfg->data,
                             cfg->size);

    if (error) {
        dev_err(&ts->client->dev, "Failed to write config data: %d",
                error);
        return error;
    }

    dev_dbg(&ts->client->dev, "Config sent successfully.");

    /* Let the firmware reconfigure itself, so sleep for 10ms */
    usleep_range(10000, 11000);

    return 0;
}

static int goodix_int_sync(struct goodix_ts_data *ts)
{
    int error;

    /*
    error = gpiod_direction_output(ts->gpiod_int, 0);
    if (error)
        return error;
   */

    msleep(50);             /* T5: 50ms */
    //du90ub941_948_gpio3_input(ts);
    du90ub941_948_gpio_input(ts, ts->irq_channel);
    msleep(50);

    error = gpiod_direction_input(ts->gpiod_int);

    if (error)
        return error;

    return 0;
}

/**
 * goodix_reset - Reset device during power on
 *
 * @ts: goodix_ts_data pointer
 */
static int goodix_reset(struct goodix_ts_data *ts)
{
    int error;
#if 0
    gpiod_direction_output(ts->gpiod_rst, 1);
    gpiod_direction_output(ts->gpiod_int, 1);
    msleep(100);
    gpiod_direction_output(ts->gpiod_int, 0);
#endif
    /* begin select I2C slave addr */
#if USE_RST_BYPASS
    error = gpiod_direction_output(ts->gpiod_rst, 0);
#else
    error = du90ub948_gpio_output_val(ts, ts->reset_channel, 0);
#endif

    if (error)
        return error;

    msleep(20);             /* T2: > 10ms */
//  msleep(100);

    //du90ub941_948_gpio3_output(ts->client);
    msleep(50);

    dev_err(&ts->client->dev, "ts->client->addr == 0x14  %d\n",
            (ts->client->addr == 0x14));
    /* HIGH: 0x28/0x29, LOW: 0xBA/0xBB */
    error = gpiod_direction_output(ts->gpiod_int, (ts->client->addr == 0x14));
    error = du90ub948_gpio_output_val(ts, ts->irq_channel, (ts->client->addr == 0x14));

    if (error)
        return error;

    usleep_range(100, 2000);        /* T3: > 100us */
//  msleep(200);

    dev_err(&ts->client->dev, "%s(): reset high\n", __func__);
#if USE_RST_BYPASS
    error = gpiod_direction_output(ts->gpiod_rst, 1);
#else
    error = du90ub948_gpio_output_val(ts, ts->reset_channel, 1);
#endif

    if (error)
        return error;

    usleep_range(6000, 10000);      /* T4: > 5ms */

    if(ts->client->addr == 0x14)
		error = du90ub948_gpio_output_val(ts, ts->irq_channel, 0);

    /* end select I2C slave addr */
//  error = gpiod_direction_input(ts->gpiod_rst);
    if (error)
        return error;

    error = goodix_int_sync(ts);

    if (error)
        return error;

    return 0;
}

/**
 * goodix_get_gpio_config - Get GPIO config from ACPI/DT
 *
 * @ts: goodix_ts_data pointer
 */
static int goodix_get_gpio_config(struct goodix_ts_data *ts)
{
    int error;
    struct device *dev;
    struct gpio_desc *gpiod;

    if (!ts->client)
        return -EINVAL;

    dev = &ts->client->dev;

    /* Get the interrupt GPIO pin number */
    gpiod = devm_gpiod_get_optional(dev, GOODIX_GPIO_INT_NAME, GPIOD_IN);

    if (IS_ERR(gpiod)) {
        error = PTR_ERR(gpiod);

        if (error != -EPROBE_DEFER)
            dev_err(dev, "Failed to get %s GPIO: %d\n",
                    GOODIX_GPIO_INT_NAME, error);

        return error;
    }

    ts->gpiod_int = gpiod;

#if USE_RST_BYPASS
    /* Get the reset line GPIO pin number */
    gpiod = devm_gpiod_get_optional(dev, GOODIX_GPIO_RST_NAME, GPIOD_IN);

    if (IS_ERR(gpiod)) {
        error = PTR_ERR(gpiod);

        if (error != -EPROBE_DEFER)
            dev_err(dev, "Failed to get %s GPIO: %d\n",
                    GOODIX_GPIO_RST_NAME, error);

        return error;
    }

    ts->gpiod_rst = gpiod;
#endif

    return 0;
}

/**
 * goodix_read_config - Read the embedded configuration of the panel
 *
 * @ts: our goodix_ts_data pointer
 *
 * Must be called during probe
 */
static void goodix_read_config(struct goodix_ts_data *ts)
{
    u8 config[GOODIX_CONFIG_MAX_LENGTH];
    int error;

    error = goodix_i2c_read(ts->client, GOODIX_REG_CONFIG_DATA,
                            config, ts->cfg_len);

    if (error) {
        dev_warn(&ts->client->dev,
                 "Error reading config (%d), using defaults\n",
                 error);
        ts->abs_x_max = GOODIX_MAX_WIDTH;
        ts->abs_y_max = GOODIX_MAX_HEIGHT;

        if (ts->swapped_x_y)
            swap(ts->abs_x_max, ts->abs_y_max);

        ts->int_trigger_type = GOODIX_INT_TRIGGER;
        ts->max_touch_num = GOODIX_MAX_CONTACTS;
        return;
    }

    dev_info(&ts->client->dev,
             "%s(): len=%d, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", __func__,
             ts->cfg_len, config[0], config[1], config[2], config[3], config[4],
             config[5], config[6]);
    ts->abs_x_max = get_unaligned_le16(&config[RESOLUTION_LOC]);
    ts->abs_y_max = get_unaligned_le16(&config[RESOLUTION_LOC + 2]);

    if (ts->swapped_x_y)
        swap(ts->abs_x_max, ts->abs_y_max);

    ts->int_trigger_type = config[TRIGGER_LOC] & 0x03;
    //ts->int_trigger_type = 0;
    ts->max_touch_num = config[MAX_CONTACTS_LOC] & 0x0f;

    if (!ts->abs_x_max || !ts->abs_y_max || !ts->max_touch_num) {
        dev_err(&ts->client->dev,
                "Invalid config, using defaults\n");
        ts->abs_x_max = GOODIX_MAX_WIDTH;
        ts->abs_y_max = GOODIX_MAX_HEIGHT;

        if (ts->swapped_x_y)
            swap(ts->abs_x_max, ts->abs_y_max);

        ts->max_touch_num = GOODIX_MAX_CONTACTS;
    }

    if (dmi_check_system(rotated_screen)) {
        ts->inverted_x = true;
        ts->inverted_y = true;
        dev_dbg(&ts->client->dev,
                "Applying '180 degrees rotated screen' quirk\n");
    }
}

/**
 * goodix_read_version - Read goodix touchscreen version
 *
 * @ts: our goodix_ts_data pointer
 */
static int goodix_read_version(struct goodix_ts_data *ts)
{
    int error;
    u8 buf[6];
    char id_str[5];

    error = goodix_i2c_read(ts->client, GOODIX_REG_ID, buf, sizeof(buf));

    if (error) {
        dev_err(&ts->client->dev, "read version failed: %d\n", error);
        return error;
    }

    memcpy(id_str, buf, 4);
    id_str[4] = 0;

    if (kstrtou16(id_str, 10, &ts->id))
        ts->id = 0x1001;

    ts->version = get_unaligned_le16(&buf[4]);

    dev_err(&ts->client->dev, "Touch ID: %d, version: 0x%04x\n", ts->id,
            ts->version);

    return 0;
}

/**
 * goodix_i2c_test - I2C test function to check if the device answers.
 *
 * @client: the i2c client
 */
static int goodix_i2c_test(struct i2c_client *client)
{
    int retry = 0;
    int error;
    u8 test = 0;

    while (retry++ < 2) {
        error = goodix_i2c_read(client, GOODIX_REG_CONFIG_DATA,
                                &test, 1);
        dev_err(&client->dev, "%s(): read error=%d, test=0x%x\n", __func__, error,
                test);

        if (!error)
            return 0;

        dev_err(&client->dev, "i2c test failed attempt %d: %d\n",
                retry, error);
        msleep(20);
    }

    return error;
}

/**
 * goodix_request_input_dev - Allocate, populate and register the input device
 *
 * @ts: our goodix_ts_data pointer
 *
 * Must be called during probe
 */
static int goodix_request_input_dev(struct goodix_ts_data *ts)
{
    int error;

    ts->input_dev = devm_input_allocate_device(&ts->client->dev);

    if (!ts->input_dev) {
        dev_err(&ts->client->dev, "Failed to allocate input device.");
        return -ENOMEM;
    }

    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
                         0, ts->abs_x_max, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
                         0, ts->abs_y_max, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

    input_mt_init_slots(ts->input_dev, ts->max_touch_num,
                        INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);

    ts->input_dev->name = "Goodix Capacitive TouchScreen";

    if (ts->dev_type == 1)
        ts->input_dev->phys = "input/ts_aux";
    else
        ts->input_dev->phys = "input/ts_main";

    ts->input_dev->id.bustype = BUS_I2C;
    ts->input_dev->id.vendor = 0x0416;
    ts->input_dev->id.product = ts->id;
    ts->input_dev->id.version = ts->version;

    /* Capacitive Windows/Home button on some devices */
    input_set_capability(ts->input_dev, EV_KEY, KEY_LEFTMETA);

    error = input_register_device(ts->input_dev);

    if (error) {
        dev_err(&ts->client->dev,
                "Failed to register input device: %d", error);
        return error;
    }

    return 0;
}

#if TP_USE_THREAD
static int goodix_tp_thread(void *data)
{
    printk("%s\n", __func__);
    struct goodix_ts_data *ts = data;

    while (1) {
        msleep(10);
        goodix_process_events(ts);

        if (goodix_i2c_write_u8(ts->client, GOODIX_READ_COOR_ADDR, 0) < 0)
            dev_err(&ts->client->dev, "I2C write end_cmd error\n");
    }

    return 0;
}

#endif
/**
 * goodix_configure_dev - Finish device initialization
 *
 * @ts: our goodix_ts_data pointer
 *
 * Must be called from probe to finish initialization of the device.
 * Contains the common initialization code for both devices that
 * declare gpio pins and devices that do not. It is either called
 * directly from probe or from request_firmware_wait callback.
 */
static int goodix_configure_dev(struct goodix_ts_data *ts)
{
    int error;
    printk("%s\n", __func__);
    ts->swapped_x_y = device_property_read_bool(&ts->client->dev,
                      "touchscreen-swapped-x-y");
    ts->inverted_x = device_property_read_bool(&ts->client->dev,
                     "touchscreen-inverted-x");
    ts->inverted_y = device_property_read_bool(&ts->client->dev,
                     "touchscreen-inverted-y");

    goodix_read_config(ts);

    error = goodix_request_input_dev(ts);

    if (error)
        return error;

    ts->irq_flags = goodix_irq_flags[ts->int_trigger_type] | IRQF_ONESHOT;

#if TP_USE_THREAD
    kthread_run(goodix_tp_thread, ts, "tp thread");
#else
    ts->client->irq = gpiod_to_irq(ts->gpiod_int);
    error = goodix_request_irq(ts);

    if (error) {
        dev_err(&ts->client->dev, "request IRQ failed: %d\n", error);
        return error;
    }

#endif
    return 0;
}

/**
 * goodix_config_cb - Callback to finish device init
 *
 * @ts: our goodix_ts_data pointer
 *
 * request_firmware_wait callback that finishes
 * initialization of the device.
 */
static void goodix_config_cb(const struct firmware *cfg, void *ctx)
{
    struct goodix_ts_data *ts = ctx;
    int error;

    if (cfg) {
        /* send device configuration to the firmware */
        error = goodix_send_cfg(ts, cfg);

        if (error)
            goto err_release_cfg;
    }

    goodix_configure_dev(ts);

err_release_cfg:
    release_firmware(cfg);
    complete_all(&ts->firmware_loading_complete);
}

static int goodix_ts_probe_rest(struct goodix_ts_data *ts)
{
	int error;
	u8 dreg, dval;
	int count = 0;
	struct i2c_client *client = ts->client;

	while (1) {
		dreg = 0x5a;
		du90ub941_i2c_read(ts, dreg, &dval, 1);
		if (dval == 0xd9 || count >= 20) {
			dev_err(&client->dev, "serdes link count=%d\n",count);
			break;
		}
		count++;
		msleep(100);
	}

	du90ub941_enable_port(ts, ts->port_type);

#if 1
	dreg = 0x17;
	dval = 0x1e | 0x80;
	du90ub941_i2c_write(ts, dreg, &dval, 1);
	dreg = 0x7;
	dval = 0x5d << 1;
	du90ub941_i2c_write(ts, dreg, &dval, 1);
	dreg = 0x8;
	dval = 0x5d << 1;
	du90ub941_i2c_write(ts, dreg, &dval, 1);
	dreg = 0x70;
	dval = 0x14 << 1;
	du90ub941_i2c_write(ts, dreg, &dval, 1);
	dreg = 0x77;
	dval = 0x14 << 1;
	du90ub941_i2c_write(ts, dreg, &dval, 1);

	dreg = 0x17;
	dval = 0;
	du90ub941_i2c_read(ts, dreg, &dval, 1);
	dev_err(&client->dev, "941 reg=0x%x, val=0x%x\n", dreg, dval);
	dreg = 0x7;
	dval = 0;
	du90ub941_i2c_read(ts, dreg, &dval, 1);
	dev_err(&client->dev, "941 reg=0x%x, val=0x%x\n", dreg, dval);
	dreg = 0x8;
	dval = 0;
	du90ub941_i2c_read(ts, dreg, &dval, 1);
	dev_err(&client->dev, "941 reg=0x%x, val=0x%x\n", dreg, dval);
	dreg = 0x70;
	dval = 0;
	du90ub941_i2c_read(ts, dreg, &dval, 1);
	dev_err(&client->dev, "941 reg=0x%x, val=0x%x\n", dreg, dval);
	dreg = 0x77;
	dval = 0;
	du90ub941_i2c_read(ts, dreg, &dval, 1);
	dev_err(&client->dev, "941 reg=0x%x, val=0x%x\n", dreg, dval);
#endif

#if USE_RST_BYPASS
	du90ub941_948_gpio2_output(ts);
#endif

	//du90ub941_948_gpio3_output(ts);
	du90ub941_948_gpio_output(ts, ts->irq_channel);
	msleep(10);

#if USE_RST_BYPASS
	if (ts->gpiod_int && ts->gpiod_rst) {
#else
	if (ts->gpiod_int) {
#endif
		/* reset the controller */
		dev_err(&client->dev, "%s(): call reset\n", __func__);

		error = goodix_reset(ts);
		if (error) {
			dev_err(&client->dev, "Controller reset failed.\n");
			return error;
		}
	}

	error = goodix_i2c_test(client);
	if (error) {
		dev_err(&client->dev, "I2C communication failure: %d\n", error);
		return error;
	}

	error = goodix_read_version(ts);
	if (error) {
		dev_err(&client->dev, "Read version failed.\n");
		return error;
	}

	ts->cfg_len = goodix_get_cfg_len(ts->id);

#if USE_RST_BYPASS
	if (ts->gpiod_int && ts->gpiod_rst) {
#else
	if (ts->gpiod_int) {
#endif
		/* update device config */
		ts->cfg_name = devm_kasprintf(&client->dev, GFP_KERNEL,
					      "goodix_%d_cfg.bin", ts->id);
		if (!ts->cfg_name)
			return -ENOMEM;

		error = request_firmware_nowait(THIS_MODULE, true, ts->cfg_name,
						&client->dev, GFP_KERNEL, ts, goodix_config_cb);
		if (error) {
			dev_err(&client->dev, "Failed to invoke firmware loader: %d\n", error);
			return error;
		}

		return 0;
	}
	else {
		dev_err(&client->dev, "%s(): do else \n", __func__);

		error = goodix_configure_dev(ts);
		if (error)
			return error;
	}

	return 0;
}

static int goodix_ts_probe_rest_thread(void *data)
{
	struct goodix_ts_data *ts = data;
	goodix_ts_probe_rest(ts);
	dev_err(&ts->client->dev, "%s end\n", __func__);
	return 0;
}

static int goodix_ts_probe(struct i2c_client *client,
                           const struct i2c_device_id *id)
{
    struct goodix_ts_data *ts;
    int error;
    u8 dreg, dval;
    u32 val;

    dev_err(&client->dev, "I2C Address: 0x%02x\n", client->addr);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "I2C check functionality failed.\n");
        return -ENXIO;
    }

    ts = devm_kzalloc(&client->dev, sizeof(*ts), GFP_KERNEL);

    if (!ts)
        return -ENOMEM;

    ts->client = client;
    i2c_set_clientdata(client, ts);
    init_completion(&ts->firmware_loading_complete);

    error = goodix_get_gpio_config(ts);
    dev_err(&client->dev, "%s(): get_gpio error=%d \n", __func__, error);

    if (error)
        return error;

    val = 0;
    error = of_property_read_u32(client->dev.of_node, "addr_ds941", &val);

    if (error < 0) {
        dev_err(&client->dev, "Missing addr_ds941\n");
    }

    ts->addr_ds941 = (u8)val;

    val = 0;
    error = of_property_read_u32(client->dev.of_node, "addr_ds948", &val);

    if (error < 0) {
        dev_err(&client->dev, "Missing addr_ds948\n");
    }

    ts->addr_ds948 = (u8)val;
    dev_err(&client->dev, "ts->addr_ds941=0x%x, ts->addr_ds948=0x%x\n",
            ts->addr_ds941, ts->addr_ds948);


    val = 0;
    error = of_property_read_u32(client->dev.of_node, "irq_channel", &val);

    if (error < 0) {
        dev_err(&client->dev, "Missing addr_ds948\n");
    }

    ts->irq_channel = val;

    val = 0;
    error = of_property_read_u32(client->dev.of_node, "reset_channel", &val);

    if (error < 0) {
        dev_err(&client->dev, "Missing addr_ds948\n");
    }

    ts->reset_channel = val;
    dev_err(&client->dev, "ts->irq_channel=%d, ts->reset_channel=%d\n",
            ts->irq_channel, ts->reset_channel);

    error = of_property_read_u32(client->dev.of_node, "dev_type",
                                 &ts->dev_type);

    if (error < 0) {
        dev_err(&client->dev, "Missing type, use default\n");
        ts->dev_type = 0;
    }

    error = of_property_read_u32(client->dev.of_node, "port_type",
                                 &ts->port_type);

    if (error < 0) {
        dev_err(&client->dev, "Missing port type, use default\n");
        ts->port_type = 1;
    }

    error = of_property_read_u32(client->dev.of_node, "thread_probe",
                                 &ts->thread_probe);

    if (error < 0) {
        dev_err(&client->dev, "Missing thread probe, use default\n");
        ts->thread_probe = 0;
    }

    dreg = 0x6;
    dval = 0x0;
    error = du90ub941_i2c_read(ts, dreg, &dval, 1);
    if (error < 0) {
        dev_err(&client->dev, "ds941 offline\n");
        return error;
    }
    else if ((dval >> 1) != ts->addr_ds948) {
        dev_err(&client->dev, "ds948 offline, addr=0x%x\n", dval >> 1);
        return -1;
    }
    else {
        dev_err(&client->dev, "ds948 online, addr=0x%x\n", dval >> 1);
    }

    if (ts->thread_probe)
        kthread_run(goodix_ts_probe_rest_thread, ts, "goodix probe thread");
    else
        error = goodix_ts_probe_rest(ts);

    return error;
}

static int goodix_ts_remove(struct i2c_client *client)
{
    struct goodix_ts_data *ts = i2c_get_clientdata(client);

#if USE_RST_BYPASS
    if (ts->gpiod_int && ts->gpiod_rst)
#else
    if (ts->gpiod_int)
#endif
        wait_for_completion(&ts->firmware_loading_complete);

    return 0;
}

static int __maybe_unused goodix_suspend(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct goodix_ts_data *ts = i2c_get_clientdata(client);
    int error;

    /* We need gpio pins to suspend/resume */
#if USE_RST_BYPASS
    if (!ts->gpiod_int || !ts->gpiod_rst) {
#else
    if (ts->gpiod_int) {
#endif
        disable_irq(client->irq);
        return 0;
    }

    wait_for_completion(&ts->firmware_loading_complete);

    /* Free IRQ as IRQ pin is used as output in the suspend sequence */
    goodix_free_irq(ts);

    /* Output LOW on the INT pin for 5 ms */
    error = gpiod_direction_output(ts->gpiod_int, 0);

    if (error) {
        goodix_request_irq(ts);
        return error;
    }

    usleep_range(5000, 6000);

    error = goodix_i2c_write_u8(ts->client, GOODIX_REG_COMMAND,
                                GOODIX_CMD_SCREEN_OFF);

    if (error) {
        dev_err(&ts->client->dev, "Screen off command failed\n");
        gpiod_direction_input(ts->gpiod_int);
        goodix_request_irq(ts);
        return -EAGAIN;
    }

    /*
     * The datasheet specifies that the interval between sending screen-off
     * command and wake-up should be longer than 58 ms. To avoid waking up
     * sooner, delay 58ms here.
     */
    msleep(58);
    return 0;
}

static int __maybe_unused goodix_resume(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct goodix_ts_data *ts = i2c_get_clientdata(client);
    int error;

    if (!ts->gpiod_int || !ts->gpiod_rst) {
        enable_irq(client->irq);
        return 0;
    }

    /*
     * Exit sleep mode by outputting HIGH level to INT pin
     * for 2ms~5ms.
     */
    error = gpiod_direction_output(ts->gpiod_int, 1);

    if (error)
        return error;

    usleep_range(2000, 5000);

    error = goodix_int_sync(ts);

    if (error)
        return error;

    error = goodix_request_irq(ts);

    if (error)
        return error;

    return 0;
}

static SIMPLE_DEV_PM_OPS(goodix_pm_ops, goodix_suspend, goodix_resume);

static const struct i2c_device_id goodix_ts_id[] = {
    { "GDIX1001:00", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, goodix_ts_id);

#ifdef CONFIG_ACPI
static const struct acpi_device_id goodix_acpi_match[] = {
    { "GDIX1001", 0 },
    { "GDIX1002", 0 },
    { }
};
MODULE_DEVICE_TABLE(acpi, goodix_acpi_match);
#endif

#ifdef CONFIG_OF
static const struct of_device_id goodix_of_match[] = {
    { .compatible = "sdrv,ds941_ds948_gt9xx" },
    { }
};
MODULE_DEVICE_TABLE(of, goodix_of_match);
#endif

static struct i2c_driver goodix_ts_driver = {
    .probe = goodix_ts_probe,
    .remove = goodix_ts_remove,
    .id_table = goodix_ts_id,
    .driver = {
        .name = "Semidrive DS941 DS948 Goodix-TS",
        .acpi_match_table = ACPI_PTR(goodix_acpi_match),
        .of_match_table = of_match_ptr(goodix_of_match),
        .pm = &goodix_pm_ops,
    },
};
module_i2c_driver(goodix_ts_driver);

MODULE_AUTHOR("Benjamin Tissoires <benjamin.tissoires@gmail.com>");
MODULE_AUTHOR("Bastien Nocera <hadess@hadess.net>");
MODULE_DESCRIPTION("Goodix touchscreen driver");
MODULE_LICENSE("GPL v2");
