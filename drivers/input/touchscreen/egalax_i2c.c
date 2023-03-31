/*
 *
 * Touch Screen I2C Driver for EETI Controller
 *
 * Copyright (C) 2000-2019  eGalax_eMPIA Technology Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define RELEASE_DATE "2019/01/19"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/kfifo.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/poll.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
#include <linux/input/mt.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
#include <linux/of_gpio.h>
#endif
#include "serdes_9xx.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
	#define __devinit
	#define __devexit
	#define __devexit_p(x) x
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	#include <linux/earlysuspend.h>
	static struct early_suspend egalax_early_suspend;
#endif

// Global define to enable function
//#define _SWITCH_XY
//#define _CONVERT_Y

#define MAX_EVENTS		600
#define MAX_I2C_LEN		64
#define FIFO_SIZE		8192 //(PAGE_SIZE*2)
#define MAX_SUPPORT_POINT	16
#define REPORTID_MOUSE		0x01
#define REPORTID_VENDOR		0x03
#define REPORTID_MTOUCH		0x06
#define MAX_RESOLUTION		4095
#define REPORTID_MTOUCH2	0x18

#define EGALAX_GPIO_INT_NAME		"irq"
#define EGALAX_GPIO_RST_NAME		"reset"

// running mode
#define MODE_STOP	0
#define MODE_WORKING	1
#define MODE_IDLE	2
#define MODE_SUSPEND	3

enum ds90ub9xx_gpio {
	GPIO0 = 0,
	GPIO1,
	GPIO2,
	GPIO3,
	GPIO4,
	GPIO5,
	GPIO6,
	GPIO7,
	GPIO8,
	DS_GPIO_NUM,
};

struct tagMTContacts {
	unsigned char ID;
	signed char Status;
	unsigned short X;
	unsigned short Y;
};

struct _egalax_i2c {
	struct workqueue_struct *ktouch_wq;
	struct work_struct work_irq;
	struct delayed_work delay_work_ioctl;
	struct mutex mutex_wq;
	struct i2c_client *client;
	unsigned char work_state;
	unsigned char skip_packet;
	unsigned int ioctl_cmd;
	struct gpio_desc *interrupt_gpio;
	struct gpio_desc *reset_gpio;
	int int_gpio;
	int rst_gpio;
	u8 addr_ds947;
    u8 addr_ds948;
};

struct egalax_char_dev
{
	int OpenCnts;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
	struct kfifo* pDataKFiFo;
#else
	struct kfifo DataKFiFo;
#endif
	unsigned char *pFiFoBuf;
	spinlock_t FiFoLock;
	struct semaphore sem;
	wait_queue_head_t fifo_inq;
};

static struct _egalax_i2c *p_egalax_i2c_dev = NULL;	// allocated in egalax_i2c_probe
static struct egalax_char_dev *p_char_dev = NULL;	// allocated in init_module
static atomic_t egalax_char_available = ATOMIC_INIT(1);
static atomic_t wait_command_ack = ATOMIC_INIT(0);
static struct input_dev *input_dev = NULL;
static struct tagMTContacts pContactBuf[MAX_SUPPORT_POINT];
static unsigned char input_report_buf[MAX_I2C_LEN+2];

#define DBG_MODULE	0x00000001
#define DBG_CDEV	0x00000002
#define DBG_PROC	0x00000004
#define DBG_POINT	0x00000008
#define DBG_INT		0x00000010
#define DBG_I2C		0x00000020
#define DBG_SUSP	0x00000040
#define DBG_INPUT	0x00000080
#define DBG_CONST	0x00000100
#define DBG_IDLE	0x00000200
#define DBG_WAKEUP	0x00000400
#define DBG_BUTTON	0x00000800
static unsigned int DbgLevel = DBG_MODULE|DBG_SUSP;

#define PROC_FS_NAME	"egalax_dbg"
#define PROC_FS_MAX_LEN	8
static struct proc_dir_entry *dbgProcFile;

#define EGALAX_DBG(level, fmt, args...)  do{ if( (level&DbgLevel)>0 ) \
					printk( KERN_DEBUG "[egalax_i2c]: " fmt, ## args); }while(0)

static int egalax_I2C_read(unsigned char *pBuf, unsigned short len)
{
	struct i2c_msg xfer;

	if(pBuf==NULL)
		return -1;

	// Read device data
	xfer.addr = p_egalax_i2c_dev->client->addr;
	xfer.flags = I2C_M_RD;
	xfer.len = len;
	xfer.buf = pBuf;

	if(i2c_transfer(p_egalax_i2c_dev->client->adapter, &xfer, 1) != 1)
	{
		EGALAX_DBG(DBG_I2C, " %s: i2c transfer fail\n", __func__);
		return -EIO;
	}
	else
		EGALAX_DBG(DBG_I2C, " %s: i2c transfer success\n", __func__);

	return 0;
}

static int egalax_I2C_write(unsigned short reg, unsigned char *pBuf, unsigned short len)
{
	unsigned char cmdbuf[4+len];
	struct i2c_msg xfer;

	if(pBuf==NULL)
		return -1;

	cmdbuf[0] = reg & 0x00FF;
	cmdbuf[1] = (reg >> 8) & 0x00FF;
	cmdbuf[2] = (len+2) & 0x00FF;
	cmdbuf[3] = ((len+2) >> 8) & 0x00FF;
	memcpy(cmdbuf+4, pBuf, len);

	// Write data to device
	xfer.addr = p_egalax_i2c_dev->client->addr;
	xfer.flags = 0;
	xfer.len = sizeof(cmdbuf);
	xfer.buf = cmdbuf;

	if(i2c_transfer(p_egalax_i2c_dev->client->adapter, &xfer, 1) != 1)
	{
		EGALAX_DBG(DBG_I2C, " %s: i2c transfer fail\n", __func__);
		return -EIO;
	}
	else
		EGALAX_DBG(DBG_I2C, " %s: i2c transfer success\n", __func__);

	return 0;
}

static int wakeup_controller(int irq)
{
	int ret = 0;

	disable_irq(irq);

	gpio_direction_output(p_egalax_i2c_dev->int_gpio, 0);
	udelay(200);
	gpio_direction_input(p_egalax_i2c_dev->int_gpio); //return to high level

	enable_irq(irq);

	EGALAX_DBG(DBG_WAKEUP, " INT wakeup touch controller done\n");

	return ret;
}

static int egalax_cdev_open(struct inode *inode, struct file *filp)
{
	if( !atomic_dec_and_test(&egalax_char_available) )
	{
		atomic_inc(&egalax_char_available);
		return -EBUSY; // already open
	}

	p_char_dev->OpenCnts++;
	filp->private_data = p_char_dev;// Used by the read and write metheds

	EGALAX_DBG(DBG_CDEV, " CDev open done!\n");
	try_module_get(THIS_MODULE);
	return 0;
}

static int egalax_cdev_release(struct inode *inode, struct file *filp)
{
	struct egalax_char_dev *cdev = filp->private_data;

	atomic_inc(&egalax_char_available); // release the device

	cdev->OpenCnts--;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
	kfifo_reset( cdev->pDataKFiFo );
#else
	kfifo_reset( &cdev->DataKFiFo );
#endif

	EGALAX_DBG(DBG_CDEV, " CDev release done!\n");
	module_put(THIS_MODULE);
	return 0;
}

static char fifo_read_buf[MAX_I2C_LEN];
static ssize_t egalax_cdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	int read_cnt, ret, fifoLen;
	struct egalax_char_dev *cdev = file->private_data;

	if( down_interruptible(&cdev->sem) )
		return -ERESTARTSYS;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
	fifoLen = kfifo_len(cdev->pDataKFiFo);
#else
	fifoLen = kfifo_len(&cdev->DataKFiFo);
#endif

	while( fifoLen<1 ) // nothing to read
	{
		up(&cdev->sem); // release the lock
		if( file->f_flags & O_NONBLOCK )
			return -EAGAIN;

	#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
		if( wait_event_interruptible(cdev->fifo_inq, kfifo_len( cdev->pDataKFiFo )>0) )
	#else
		if( wait_event_interruptible(cdev->fifo_inq, kfifo_len( &cdev->DataKFiFo )>0) )
	#endif
		{
			return -ERESTARTSYS; // signal: tell the fs layer to handle it
		}

		if( down_interruptible(&cdev->sem) )
			return -ERESTARTSYS;
	}

	if(count > MAX_I2C_LEN)
		count = MAX_I2C_LEN;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
	read_cnt = kfifo_get(cdev->pDataKFiFo, fifo_read_buf, count);
#else
	read_cnt = kfifo_out_locked(&cdev->DataKFiFo, fifo_read_buf, count, &cdev->FiFoLock);
#endif
	EGALAX_DBG(DBG_CDEV, " \"%s\" reading fifo data count=%d\n", current->comm, read_cnt);

	ret = copy_to_user(buf, fifo_read_buf, read_cnt)?-EFAULT:read_cnt;

	up(&cdev->sem);

	return ret;
}

static ssize_t egalax_cdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	struct egalax_char_dev *cdev = file->private_data;
	int ret=0;
	char *tmp;

	if( down_interruptible(&cdev->sem) )
		return -ERESTARTSYS;

	if (count > MAX_I2C_LEN)
		count = MAX_I2C_LEN;

	tmp = kzalloc(MAX_I2C_LEN, GFP_KERNEL);
	if(tmp==NULL)
	{
		up(&cdev->sem);
		return -ENOMEM;
	}

	if(copy_from_user(tmp, buf, count))
	{
		up(&cdev->sem);
		kfree(tmp);
		return -EFAULT;
	}

	ret = egalax_I2C_write(0x0067, tmp, MAX_I2C_LEN);

	up(&cdev->sem);
	EGALAX_DBG(DBG_CDEV, " I2C writing %zu bytes.\n", count);
	kfree(tmp);

	return (ret==0?count:-1);
}

static unsigned int egalax_cdev_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct egalax_char_dev *cdev = filp->private_data;
	unsigned int mask = 0;
	int fifoLen;

	down(&cdev->sem);
	poll_wait(filp, &cdev->fifo_inq,  wait);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
	fifoLen = kfifo_len(cdev->pDataKFiFo);
#else
	fifoLen = kfifo_len(&cdev->DataKFiFo);
#endif

	if( fifoLen > 0 )
		mask |= POLLIN | POLLRDNORM;    /* readable */
	if( (FIFO_SIZE - fifoLen) > MAX_I2C_LEN )
		mask |= POLLOUT | POLLWRNORM;   /* writable */

	up(&cdev->sem);
	return mask;
}

static int egalax_proc_show(struct seq_file* seqfilp, void *v)
{
	seq_printf(seqfilp, "EETI I2C for All Points.\nDebug Level: 0x%08X\nRelease Date: %s\n", DbgLevel, RELEASE_DATE);
	return 0;
}

static int egalax_proc_open(struct inode *inode, struct file *filp)
{
	EGALAX_DBG(DBG_PROC, " \"%s\" call proc_open\n", current->comm);
	return single_open(filp, egalax_proc_show, NULL);
}

static ssize_t egalax_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	char procfs_buffer_size = 0;
	unsigned char procfs_buf[PROC_FS_MAX_LEN+1] = {0};

	EGALAX_DBG(DBG_PROC, " \"%s\" call proc_write\n", current->comm);

	procfs_buffer_size = count;
	if(procfs_buffer_size > PROC_FS_MAX_LEN )
		procfs_buffer_size = PROC_FS_MAX_LEN+1;

	if( copy_from_user(procfs_buf, buf, procfs_buffer_size) )
	{
		EGALAX_DBG(DBG_PROC, " proc_write faied at copy_from_user\n");
		return -EFAULT;
	}

	sscanf(procfs_buf, "%x", &DbgLevel);
	EGALAX_DBG(DBG_PROC, " Switch Debug Level to 0x%08X\n", DbgLevel);

	return procfs_buffer_size;
}

#define MAX_POINT_PER_PACKET	5
#define POINT_STRUCT_SIZE	10
static int TotalPtsCnt=0, RecvPtsCnt=0;
static void ProcessReport(unsigned char *buf, struct _egalax_i2c *p_egalax_i2c)
{
	unsigned char i, index=0, cnt_down=0, cnt_up=0, shift=0;
	unsigned char status=0;
	unsigned short contactID=0, x=0, y=0;

	if(TotalPtsCnt<=0)
	{
		if(buf[1]==0 || buf[1]>MAX_SUPPORT_POINT)
		{
			EGALAX_DBG(DBG_POINT, " NumsofContacts mismatch, skip packet\n");
			return;
		}

		TotalPtsCnt = buf[1];
		RecvPtsCnt = 0;
	}
	else if(buf[1]>0)
	{
		TotalPtsCnt = RecvPtsCnt = 0;
		EGALAX_DBG(DBG_POINT, " NumsofContacts mismatch, skip packet\n");
		return;
	}

	while(index<MAX_POINT_PER_PACKET)
	{
		shift = index * POINT_STRUCT_SIZE + 2;
		status = buf[shift] & 0x01;
		contactID = buf[shift+1];
		x = ((buf[shift+3]<<8) + buf[shift+2]);
		y = ((buf[shift+5]<<8) + buf[shift+4]);

		if( buf[0]==REPORTID_MTOUCH2 )
		{
			x >>= 2;
			y >>= 2;
		}

		if( contactID>=MAX_SUPPORT_POINT )
		{
			TotalPtsCnt = RecvPtsCnt = 0;
			EGALAX_DBG(DBG_POINT, " Get error ContactID.\n");
			return;
		}

		EGALAX_DBG(DBG_MODULE, " Get Point[%d] Update:TotalPtsCnt:%d, RecvPtsCnt:%d, Status=%d X=%d Y=%d\n", contactID, TotalPtsCnt, RecvPtsCnt, status, x, y);

	#ifdef _SWITCH_XY
		EGALAX_DBG(DBG_MODULE, "_SWITCH_XY is Enable\n");
		short tmp = x;
		x = y;
		y = tmp;
	#else
		EGALAX_DBG(DBG_MODULE, "_SWITCH_XY is Not Enable\n");
	#endif

	#ifdef _CONVERT_X
		EGALAX_DBG(DBG_MODULE, "_CONVERT_X is Enable\n");
		x = MAX_RESOLUTION-x;
	#else
		EGALAX_DBG(DBG_MODULE, "_CONVERT_X is Not Enable\n");
	#endif

	#ifdef _CONVERT_Y
		EGALAX_DBG(DBG_MODULE, "_CONVERT_Y is Enable\n");
		y = MAX_RESOLUTION-y;
	#else
		EGALAX_DBG(DBG_MODULE, "_CONVERT_Y is Not Enable\n");
	#endif

		pContactBuf[RecvPtsCnt].ID = contactID;
		pContactBuf[RecvPtsCnt].Status = status;
		pContactBuf[RecvPtsCnt].X = x;
		pContactBuf[RecvPtsCnt].Y = y;

		RecvPtsCnt++;
		index++;

		// Recv all points, send input report
		if(RecvPtsCnt==TotalPtsCnt)
		{
			for(i=0; i<RecvPtsCnt; i++)
			{
			#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
				input_mt_slot(input_dev, pContactBuf[i].ID);
				input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, pContactBuf[i].Status);
				if(pContactBuf[i].Status)
				{
					input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, pContactBuf[i].Status);
					input_report_abs(input_dev, ABS_MT_POSITION_X, pContactBuf[i].X);
					input_report_abs(input_dev, ABS_MT_POSITION_Y, pContactBuf[i].Y);
				}
			#else
				input_report_abs(input_dev, ABS_MT_TRACKING_ID, pContactBuf[i].ID);
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, pContactBuf[i].Status);
				input_report_abs(input_dev, ABS_MT_POSITION_X, pContactBuf[i].X);
				input_report_abs(input_dev, ABS_MT_POSITION_Y, pContactBuf[i].Y);
				input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR, 0);
				input_mt_sync(input_dev);
			#endif

				if(pContactBuf[i].Status)
					cnt_down++;
				else
					cnt_up++;
			}
		#ifndef CONFIG_HAS_EARLYSUSPEND //We use this config to distinguish Linux and Android
			#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
			input_mt_report_pointer_emulation(input_dev, true);
			#endif
		#endif
			input_sync(input_dev);
			EGALAX_DBG(DBG_POINT, " Input sync point data done! (Down:%d Up:%d)\n", cnt_down, cnt_up);

			TotalPtsCnt = RecvPtsCnt = 0;
			return;
		}
	}
}

static struct input_dev * allocate_Input_Dev(void)
{
	int ret;
	struct input_dev *pInputDev=NULL;

	pInputDev = input_allocate_device();
	if(pInputDev == NULL)
	{
		EGALAX_DBG(DBG_MODULE, " Failed to allocate input device\n");
		return NULL;//-ENOMEM;
	}

	pInputDev->name = "eGalax_Touch_Screen";
	pInputDev->phys = "I2C";
	pInputDev->id.bustype = BUS_I2C;
	pInputDev->id.vendor = 0x0EEF;
	pInputDev->id.product = 0x0020;
	pInputDev->id.version = 0x0001;

	set_bit(EV_ABS, pInputDev->evbit);
#ifndef CONFIG_HAS_EARLYSUSPEND //We use this config to distinguish Linux and Android
	set_bit(EV_KEY, pInputDev->evbit);
	__set_bit(BTN_TOUCH, pInputDev->keybit);
	input_set_abs_params(pInputDev, ABS_X, 0, MAX_RESOLUTION, 0, 0);
	input_set_abs_params(pInputDev, ABS_Y, 0, MAX_RESOLUTION, 0, 0);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
	__set_bit(INPUT_PROP_DIRECT, pInputDev->propbit);
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
		input_mt_init_slots(pInputDev, MAX_SUPPORT_POINT, 0);
	#else
		input_mt_init_slots(pInputDev, MAX_SUPPORT_POINT);
	#endif
	input_set_abs_params(pInputDev, ABS_MT_POSITION_X, 0, MAX_RESOLUTION, 0, 0);
	input_set_abs_params(pInputDev, ABS_MT_POSITION_Y, 0, MAX_RESOLUTION, 0, 0);
	input_set_abs_params(pInputDev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
#else
	input_set_abs_params(pInputDev, ABS_MT_POSITION_X, 0, MAX_RESOLUTION, 0, 0);
	input_set_abs_params(pInputDev, ABS_MT_POSITION_Y, 0, MAX_RESOLUTION, 0, 0);
	input_set_abs_params(pInputDev, ABS_MT_WIDTH_MAJOR, 0, MAX_RESOLUTION, 0, 0); //Size
	input_set_abs_params(pInputDev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0); //Pressure
	input_set_abs_params(pInputDev, ABS_MT_TRACKING_ID, 0, MAX_SUPPORT_POINT, 0, 0);
#endif // #if LINUX_VERSION_CODE > KERNEL_VERSION(3,0,0)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	input_set_events_per_packet(pInputDev, MAX_EVENTS);
#endif

	ret = input_register_device(pInputDev);
	if(ret)
	{
		EGALAX_DBG(DBG_MODULE, " Unable to register input device.\n");
		input_free_device(pInputDev);
		pInputDev = NULL;
	}

	return pInputDev;
}

static int egalax_i2c_measure(struct _egalax_i2c *egalax_i2c)
{
	int ret=0, frameLen=0, loop=3, i;

	EGALAX_DBG(DBG_MODULE, " egalax_i2c_measure\n");

	if( egalax_I2C_read(input_report_buf, MAX_I2C_LEN+2) < 0)
	{
		EGALAX_DBG(DBG_I2C, " I2C read input report fail!\n");
		return -1;
	}

	if( DbgLevel&DBG_I2C )
	{
		char dbgmsg[(MAX_I2C_LEN+2)*4];
		for(i=0; i<MAX_I2C_LEN+2; i++)
			sprintf(dbgmsg+i*4, "[%02X]", input_report_buf[i]);
		EGALAX_DBG(DBG_I2C, " Buf=%s\n", dbgmsg);
	}

	frameLen = input_report_buf[0] + (input_report_buf[1]<<8);
	EGALAX_DBG(DBG_MODULE, " I2C read data with Len=%d\n", frameLen);

	if(frameLen==0)
	{
		EGALAX_DBG(DBG_MODULE, " Device reset\n");
		return -1;
	}

	switch(input_report_buf[2])
	{
		case REPORTID_MTOUCH:
		case REPORTID_MTOUCH2:
			if( !egalax_i2c->skip_packet && egalax_i2c->work_state==MODE_WORKING )
				ProcessReport(input_report_buf+2, egalax_i2c);
			ret = 0;
			break;
		case REPORTID_VENDOR:
			atomic_set(&wait_command_ack, 1);
			EGALAX_DBG(DBG_I2C, " I2C get vendor command packet\n");

			if( p_char_dev->OpenCnts>0 ) // If someone reading now! put the data into the buffer!
			{
				loop=3;
				do {
				#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
					ret = wait_event_timeout(p_char_dev->fifo_inq, (FIFO_SIZE-kfifo_len(p_char_dev->pDataKFiFo))>=MAX_I2C_LEN, HZ);
				#else
					ret = wait_event_timeout(p_char_dev->fifo_inq, kfifo_avail(&p_char_dev->DataKFiFo)>=MAX_I2C_LEN, HZ);
				#endif
				}while(ret<=0 && --loop);

				if(ret>0) // fifo size is ready
				{
				#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
					ret = kfifo_put(p_char_dev->pDataKFiFo, input_report_buf+2, MAX_I2C_LEN);
				#else
					ret = kfifo_in_locked(&p_char_dev->DataKFiFo, input_report_buf+2, MAX_I2C_LEN, &p_char_dev->FiFoLock);
				#endif

					wake_up_interruptible( &p_char_dev->fifo_inq );
				}
				else
					EGALAX_DBG(DBG_CDEV, " [Warning] Can't write data because fifo size is overflow.\n");
			}

			break;
		default:
			EGALAX_DBG(DBG_I2C, " I2C read error data with hedaer=%d\n", input_report_buf[2]);
			ret = -1;
			break;
	}

	return ret;
}

static void egalax_i2c_wq_irq(struct work_struct *work)
{
	struct _egalax_i2c *egalax_i2c = container_of(work, struct _egalax_i2c, work_irq);
	struct i2c_client *client = egalax_i2c->client;

	EGALAX_DBG(DBG_MODULE, " egalax_i2c_wq run\n");

	/*continue recv data*/
	while( !gpio_get_value(egalax_i2c->int_gpio) )
	{
		egalax_i2c_measure(egalax_i2c);
		schedule();
	}

	if( egalax_i2c->skip_packet > 0 )
		egalax_i2c->skip_packet = 0;

	enable_irq(client->irq);

	EGALAX_DBG(DBG_MODULE, " egalax_i2c_wq leave\n");
}

static irqreturn_t egalax_i2c_interrupt(int irq, void *dev_id)
{
	struct _egalax_i2c *egalax_i2c = (struct _egalax_i2c *)dev_id;

	EGALAX_DBG(DBG_MODULE, " INT with irq:%d\n", irq);

	disable_irq_nosync(irq);

	queue_work(egalax_i2c->ktouch_wq, &egalax_i2c->work_irq);

	return IRQ_HANDLED;
}

static void egalax_i2c_senduppoint(void)
{
	int i=0;

	EGALAX_DBG(DBG_MODULE, " %s\n", __func__);

	for(i=0; i<MAX_SUPPORT_POINT; i++)
	{
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
		input_mt_slot(input_dev, pContactBuf[i].ID);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	#else
		input_report_abs(input_dev, ABS_MT_TRACKING_ID, pContactBuf[i].ID);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_mt_sync(input_dev);
	#endif

		if(pContactBuf[i].Status)
			pContactBuf[i].Status = 0;
	}

#ifndef CONFIG_HAS_EARLYSUSPEND //We use this config to distinguish Linux and Android
	input_mt_report_pointer_emulation(input_dev, true);
#endif
	input_sync(input_dev);
	EGALAX_DBG(DBG_POINT, " Sent up point data done!\n");
}

static int egalax_i2c_pm_suspend(struct i2c_client *client, pm_message_t mesg)
{
	unsigned char cmdbuf[4];
	struct i2c_msg xfer;

	EGALAX_DBG(DBG_MODULE, " Enter pm_suspend state:%d\n", p_egalax_i2c_dev->work_state);

	if(!p_egalax_i2c_dev)
		goto fail_suspend;

	//Power sleep command
	cmdbuf[0] = 0xA7;	cmdbuf[1] = 0x00;
	cmdbuf[2] = 0x01;	cmdbuf[3] = 0x08;

	// Write data to device
	xfer.addr = p_egalax_i2c_dev->client->addr;
	xfer.flags = 0;
	xfer.len = sizeof(cmdbuf);
	xfer.buf = cmdbuf;

	if(i2c_transfer(p_egalax_i2c_dev->client->adapter, &xfer, 1) != 1)
	{
		EGALAX_DBG(DBG_MODULE, " %s: i2c send Power command fail\n", __func__);
		goto fail_suspend2;
	}

	p_egalax_i2c_dev->work_state = MODE_SUSPEND;

	EGALAX_DBG(DBG_MODULE, " pm_suspend done!!\n");
	return 0;

fail_suspend2:
	p_egalax_i2c_dev->work_state = MODE_SUSPEND;
fail_suspend:
	EGALAX_DBG(DBG_MODULE, " pm_suspend failed!!\n");
	return -1;
}

static int egalax_i2c_pm_resume(struct i2c_client *client)
{
	unsigned char cmdbuf[4];
        struct i2c_msg xfer;

	EGALAX_DBG(DBG_MODULE, " Enter pm_resume state:%d\n", p_egalax_i2c_dev->work_state);

	if(!p_egalax_i2c_dev)
		goto fail_resume;

	if( wakeup_controller(p_egalax_i2c_dev->client->irq)==0 )
	{
		//Power wakeup command
		cmdbuf[0] = 0xA7;       cmdbuf[1] = 0x00;
		cmdbuf[2] = 0x00;       cmdbuf[3] = 0x08;

		// Write data to device
		xfer.addr = p_egalax_i2c_dev->client->addr;
		xfer.flags = 0;
		xfer.len = sizeof(cmdbuf);
		xfer.buf = cmdbuf;

		if(i2c_transfer(p_egalax_i2c_dev->client->adapter, &xfer, 1) != 1)
		{
			EGALAX_DBG(DBG_I2C, " %s: i2c send Power command fail\n", __func__);
			goto fail_resume2;
		}

		p_egalax_i2c_dev->work_state = MODE_WORKING;
	}
	else
	{
		goto fail_resume2;
	}

	egalax_i2c_senduppoint();

	EGALAX_DBG(DBG_MODULE, " pm_resume done!!\n");
	return 0;

fail_resume2:
	p_egalax_i2c_dev->work_state = MODE_WORKING;
fail_resume:
	EGALAX_DBG(DBG_MODULE, " pm_resume failed!!\n");
	return -1;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void egalax_i2c_early_suspend(struct early_suspend *handler)
{
	pm_message_t state;
	state.event = PM_EVENT_SUSPEND;

	EGALAX_DBG(DBG_MODULE, " %s\n", __func__);
	egalax_i2c_pm_suspend(p_egalax_i2c_dev->client, state);
}

static void egalax_i2c_early_resume(struct early_suspend *handler)
{
	EGALAX_DBG(DBG_SUSP, " %s\n", __func__);
	egalax_i2c_pm_resume(p_egalax_i2c_dev->client);
}
#endif // #ifdef CONFIG_HAS_EARLYSUSPEND


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
static int egalax_i2c_ops_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	pm_message_t state;
	state.event = PM_EVENT_SUSPEND;
	EGALAX_DBG(DBG_SUSP, " %s\n", __func__);
	return egalax_i2c_pm_suspend(client, state);
}

static int egalax_i2c_ops_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	EGALAX_DBG(DBG_MODULE, " %s\n", __func__);
	return egalax_i2c_pm_resume(client);
}

static SIMPLE_DEV_PM_OPS(egalax_i2c_pm_ops, egalax_i2c_ops_suspend, egalax_i2c_ops_resume);
#endif

static int __devinit egalax_i2c_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	int ret;
	int error;
	struct device *dev;
	u32 val;

	EGALAX_DBG(DBG_MODULE, " Start probe\n");
	EGALAX_DBG(DBG_MODULE, "Start egalax_i2c_probe \n");
	EGALAX_DBG(DBG_MODULE, "#############################\n");
	EGALAX_DBG(DBG_MODULE, "egalax-I2C Address: 0x%02x\n", client->addr);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "I2C check functionality failed.\n");
        return -ENXIO;
    }

	p_egalax_i2c_dev = (struct _egalax_i2c *)kzalloc(sizeof(struct _egalax_i2c), GFP_KERNEL);
	if (!p_egalax_i2c_dev)
	{
		EGALAX_DBG(DBG_MODULE, " Request memory failed\n");
		ret = -ENOMEM;
		goto fail1;
	}

	dev = &client->dev;
	p_egalax_i2c_dev->client = client;
	i2c_set_clientdata(client, p_egalax_i2c_dev);

	val = 0;
	error = of_property_read_u32(client->dev.of_node, "addr_ds947", &val);

	if (error < 0) {
	    dev_err(&client->dev, "Missing addr_ds947\n");
	}

	p_egalax_i2c_dev->addr_ds947 = (u8)val;
	val = 0;
	error = of_property_read_u32(client->dev.of_node, "addr_ds948", &val);

	if (error < 0) {
	    dev_err(&client->dev, "Missing addr_ds948\n");
	}

	p_egalax_i2c_dev->addr_ds948 = (u8)val;
	dev_err(&client->dev, "egalax_i2c->addr_ds947=0x%x, egalax_i2c->addr_ds948=0x%x\n",
	        p_egalax_i2c_dev->addr_ds947, p_egalax_i2c_dev->addr_ds948);


	/* Get the interrupt GPIO pin number */
	p_egalax_i2c_dev->interrupt_gpio = devm_gpiod_get_optional(dev, EGALAX_GPIO_INT_NAME, GPIOD_IN);

	if (IS_ERR(p_egalax_i2c_dev->interrupt_gpio)) {
	    error = PTR_ERR(p_egalax_i2c_dev->interrupt_gpio);

	    if (error != -EPROBE_DEFER)
	        dev_err(dev, "Failed to get %s GPIO: %d\n",
	                EGALAX_GPIO_INT_NAME, error);

	    return error;
	}

	/* Get the interrupt GPIO pin number */
	p_egalax_i2c_dev->reset_gpio = devm_gpiod_get_optional(dev, EGALAX_GPIO_RST_NAME, GPIOD_IN);

	if (IS_ERR(p_egalax_i2c_dev->reset_gpio)) {
		error = PTR_ERR(p_egalax_i2c_dev->reset_gpio);

		if (error != -EPROBE_DEFER)
			dev_err(dev, "Failed to get %s GPIO: %d\n",
					EGALAX_GPIO_INT_NAME, error);

		return error;
	}

	p_egalax_i2c_dev->int_gpio = desc_to_gpio(p_egalax_i2c_dev->interrupt_gpio);
	p_egalax_i2c_dev->rst_gpio = desc_to_gpio(p_egalax_i2c_dev->reset_gpio);

	EGALAX_DBG(DBG_MODULE, "int_gpio:%d, rst_gpio:%d\n", p_egalax_i2c_dev->int_gpio, p_egalax_i2c_dev->rst_gpio);

	if( !gpio_is_valid(p_egalax_i2c_dev->int_gpio) )
	{
		ret = -ENODEV;
		printk("gpio %d is valid\n", p_egalax_i2c_dev->int_gpio);
		goto fail1;
	}

	ret = gpio_request(p_egalax_i2c_dev->int_gpio, "Touch IRQ");
	if(ret<0 && ret!=-EBUSY)
	{
		EGALAX_DBG(DBG_MODULE, " gpio_request[%d] failed: %d\n", p_egalax_i2c_dev->int_gpio, ret);
		goto fail1;
	}

	if( !gpio_is_valid(p_egalax_i2c_dev->rst_gpio) )
	{
		ret = -ENODEV;
		printk("gpio %d is valid\n", p_egalax_i2c_dev->rst_gpio);
		goto fail1;
	}

	du90ub941_or_947_enable_i2c_passthrough(client, p_egalax_i2c_dev->addr_ds947);
	gpiod_direction_input(p_egalax_i2c_dev->interrupt_gpio);
	du90ub948_gpio_input(client, p_egalax_i2c_dev->addr_ds948, GPIO2);
	du90ub941_or_947_gpio_input(client, p_egalax_i2c_dev->addr_ds947, GPIO2);

	gpiod_direction_output(p_egalax_i2c_dev->reset_gpio, 1);
	du90ub948_gpio_output(client, p_egalax_i2c_dev->addr_ds948, GPIO5, 1);
	msleep(5);
	gpiod_direction_output(p_egalax_i2c_dev->reset_gpio, 0);
	du90ub948_gpio_output(client, p_egalax_i2c_dev->addr_ds948, GPIO5, 0);
	msleep(10);
	gpiod_direction_output(p_egalax_i2c_dev->reset_gpio, 1);
	du90ub948_gpio_output(client, p_egalax_i2c_dev->addr_ds948, GPIO5, 1);
	msleep(50);

	input_dev = allocate_Input_Dev();
	if(input_dev==NULL)
	{
		EGALAX_DBG(DBG_MODULE, "allocate_Input_Dev failed\n");
		ret = -EINVAL;
		goto fail2;
	}
	EGALAX_DBG(DBG_MODULE, "Register input device done\n");

	mutex_init(&p_egalax_i2c_dev->mutex_wq);

	p_egalax_i2c_dev->ktouch_wq = create_singlethread_workqueue("egalax_touch_wq");
	INIT_WORK(&p_egalax_i2c_dev->work_irq, egalax_i2c_wq_irq);

	if(gpio_get_value(p_egalax_i2c_dev->int_gpio) )
		p_egalax_i2c_dev->skip_packet = 0;
	else
		p_egalax_i2c_dev->skip_packet = 1;

	p_egalax_i2c_dev->work_state = MODE_WORKING;
	p_egalax_i2c_dev->client->irq = gpiod_to_irq(p_egalax_i2c_dev->interrupt_gpio);

	ret = request_irq(p_egalax_i2c_dev->client->irq, egalax_i2c_interrupt, IRQF_TRIGGER_LOW, client->name, p_egalax_i2c_dev);
	if( ret )
	{
		EGALAX_DBG(DBG_MODULE, " Request irq(%d) failed %s\n", client->irq, client->name);
		goto fail3;
	}

	EGALAX_DBG(DBG_MODULE, " Request irq(%d) gpio(%d) with result:%d\n", client->irq, p_egalax_i2c_dev->int_gpio, ret);

#ifdef CONFIG_HAS_EARLYSUSPEND
	egalax_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	egalax_early_suspend.suspend = egalax_i2c_early_suspend;
	egalax_early_suspend.resume = egalax_i2c_early_resume;
	register_early_suspend(&egalax_early_suspend);
	EGALAX_DBG(DBG_MODULE, " Register early_suspend done\n");
#endif

	EGALAX_DBG(DBG_MODULE, "I2C probe done\n");
	return 0;

fail3:
	i2c_set_clientdata(client, NULL);
	destroy_workqueue(p_egalax_i2c_dev->ktouch_wq);
	free_irq(client->irq, p_egalax_i2c_dev);
	input_unregister_device(input_dev);
	input_dev = NULL;
fail2:
	gpio_free(p_egalax_i2c_dev->int_gpio);
	gpio_free(p_egalax_i2c_dev->rst_gpio);
fail1:
	kfree(p_egalax_i2c_dev);
	p_egalax_i2c_dev = NULL;

	EGALAX_DBG(DBG_MODULE, " I2C probe failed\n");
	return ret;
}

static int __devexit egalax_i2c_remove(struct i2c_client *client)
{
	struct _egalax_i2c *egalax_i2c = i2c_get_clientdata(client);

	egalax_i2c->work_state = MODE_STOP;

	cancel_work_sync(&egalax_i2c->work_irq);

	if(client->irq)
	{
		disable_irq(client->irq);
		free_irq(client->irq, egalax_i2c);
	}

	gpio_free(egalax_i2c->int_gpio);
	gpio_free(egalax_i2c->rst_gpio);

	if(egalax_i2c->ktouch_wq)
		destroy_workqueue(egalax_i2c->ktouch_wq);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&egalax_early_suspend);
#endif

	if(input_dev)
	{
		EGALAX_DBG(DBG_MODULE,  " Unregister input device\n");
		input_unregister_device(input_dev);
		input_dev = NULL;
	}

	i2c_set_clientdata(client, NULL);
	kfree(egalax_i2c);
	p_egalax_i2c_dev = NULL;

	return 0;
}

static const struct i2c_device_id egalax_i2c_idtable[] = {
	{ "egalax_i2c", 0 },
	{ }
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
static const struct of_device_id egalax_i2c_dt_ids[] = {
	{ .compatible = "eeti,egalax_i2c" },
	{ }
};
#endif

MODULE_DEVICE_TABLE(i2c, egalax_i2c_idtable);

static struct i2c_driver egalax_i2c_driver = {
	.driver = {
		.name 	= "egalax_i2c",
		.owner	= THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = egalax_i2c_dt_ids,
	#endif
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
		.pm		= &egalax_i2c_pm_ops,
	#endif
	},
	.id_table	= egalax_i2c_idtable,
	.probe		= egalax_i2c_probe,
	.remove		= __devexit_p(egalax_i2c_remove),
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
	#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend 	= egalax_i2c_pm_suspend,
	.resume 	= egalax_i2c_pm_resume,
	#endif
#endif

};

static const struct file_operations egalax_cdev_fops = {
	.owner	= THIS_MODULE,
	.read	= egalax_cdev_read,
	.write	= egalax_cdev_write,
	.open	= egalax_cdev_open,
	.release= egalax_cdev_release,
	.poll	= egalax_cdev_poll,
};

static const struct file_operations egalax_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= egalax_proc_open,
	.read		= seq_read,
	.write		= egalax_proc_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct miscdevice egalax_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "egalax_i2c",
	.fops = &egalax_cdev_fops,
};

static void egalax_i2c_ts_exit(void)
{
	if(p_char_dev)
	{
		if( p_char_dev->pFiFoBuf )
			kfree(p_char_dev->pFiFoBuf);

		kfree(p_char_dev);
		p_char_dev = NULL;
	}

	misc_deregister(&egalax_misc_dev);

	i2c_del_driver(&egalax_i2c_driver);

	remove_proc_entry(PROC_FS_NAME, NULL);

	EGALAX_DBG(DBG_MODULE, " Exit driver done!\n");
}

static struct egalax_char_dev* setup_chardev(void)
{
	struct egalax_char_dev *pCharDev;

	pCharDev = kzalloc(1*sizeof(struct egalax_char_dev), GFP_KERNEL);
	if(!pCharDev)
		goto fail_cdev;

	spin_lock_init( &pCharDev->FiFoLock );
	pCharDev->pFiFoBuf = kzalloc(sizeof(unsigned char)*FIFO_SIZE, GFP_KERNEL);
	if(!pCharDev->pFiFoBuf)
		goto fail_fifobuf;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
	pCharDev->pDataKFiFo = kfifo_init(pCharDev->pFiFoBuf, FIFO_SIZE, GFP_KERNEL, &pCharDev->FiFoLock);
	if( pCharDev->pDataKFiFo==NULL )
		goto fail_kfifo;
#else
	kfifo_init(&pCharDev->DataKFiFo, pCharDev->pFiFoBuf, FIFO_SIZE);
	if( !kfifo_initialized(&pCharDev->DataKFiFo) )
		goto fail_kfifo;
#endif

	pCharDev->OpenCnts = 0;
	sema_init(&pCharDev->sem, 1);
	init_waitqueue_head(&pCharDev->fifo_inq);

	return pCharDev;

fail_kfifo:
	kfree(pCharDev->pFiFoBuf);
fail_fifobuf:
	kfree(pCharDev);
fail_cdev:
	return NULL;
}

static int egalax_i2c_ts_init(void)
{
	int result;

	printk("egalax_i2c_ts_init into\n");
	result = misc_register(&egalax_misc_dev);
	if(result)
	{
		EGALAX_DBG(DBG_MODULE, " misc device register failed\n");
		goto fail;
	}

	p_char_dev = setup_chardev(); // allocate the character device
	if(!p_char_dev)
	{
		result = -ENOMEM;
		goto fail;
	}

	dbgProcFile = proc_create(PROC_FS_NAME, S_IRUGO|S_IWUGO, NULL, &egalax_proc_fops);
	if (dbgProcFile == NULL)
	{
		remove_proc_entry(PROC_FS_NAME, NULL);
		EGALAX_DBG(DBG_MODULE, " Could not initialize /proc/%s\n", PROC_FS_NAME);
	}

	EGALAX_DBG(DBG_MODULE, " Driver init done!\n");
	return i2c_add_driver(&egalax_i2c_driver);

fail:
	egalax_i2c_ts_exit();
	return result;
}

module_init(egalax_i2c_ts_init);
module_exit(egalax_i2c_ts_exit);

MODULE_AUTHOR("EETI <touch_fae@eeti.com>");
MODULE_DESCRIPTION("egalax all points controller i2c driver");
MODULE_LICENSE("GPL");
