#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/input/mt.h>

#include "siw_touch.h"
#include "serdes_9xx.h"

//#define SIW_SLOT_A

#define KEY_PRESS       1
#define KEY_RELEASE     0
#define PRESS_MAX       255

#define TS_EVENT_UNKNOWN    0x00
#define TS_EVENT_PRESS      0x01
#define TS_EVENT_MOVE       0x02
#define TS_EVENT_RELEASE    0x03

// DEVICE NAME
#define MODULE_NAME "siw_touch"

// I2C Protocol
#define IIC_CODE_START '$' //0x24

typedef enum {
	IICCMD_RESET		  = 0xA0, // Recieve size = 0 | response size = 1
	IICCMD_FWUP			  = 0xA1, // Don't Move It!!
	IICCMD_BL_RESET		  = 0xA2,
	IICCMD_GET_EVENTNUM	  = 0xA6, // Recieve size = 0 | response size = N
	IICCMD_GET_EVENT	  = 0xA7, // Recieve size = 0 | response size = N

	IICCMD_SET_MODE		  = 0xB2,
	IICCMD_SET_DEVICE	  = 0xB4,
	IICCMD_FLASH_ERASE	  = 0xB6,
	IICCMD_FLASH_SET_ADDR = 0xB7,
	IICCMD_FLASH_WRITE	  = 0xB9,
	IICCMD_FLASH_READ	  = 0xBA,

	IICCMD_DFU_ENTER	  = 0xC0,
	IICCMD_GET_BL_VER	  = 0xC7,

	IICCMD_GET_VER		  = 0xD0,
	IICCMD_GET_MODE		  = 0xD2,

	IICCMD_FW_RESET		  = 0xE1,
} IICCMD;

typedef enum {
	IIC_TARGET_MASTER     = 0x00,
	IIC_TARGET_SLAVE0     = 0x01,
	IIC_TARGET_SLAVE1     = 0x02,
	IIC_TARGET_SLAVE2     = 0x03,

	IIC_REPLY_NACK        = 0x00,
	IIC_REPLY_ACK         = 0x01,
	IIC_REPLY_ACK_FINSIH  = 0x03,
	IIC_REPLY_WAIT        = 0x04,

	IIC_REPLY_FAIL        = 0x00,
	IIC_REPLY_SUCCESS     = 0x01,

	IIC_MODE_FW_UPDATE	  = 0x44,
	IIC_MODE_USER_FW	  = 0x52,
} SiW_TouchIICParam;

typedef enum {
	FW_MODE_USER,
	FW_MODE_DFU,
	FW_MODE_UNKNOWN
} SiW_FW_Mode;

#define	SIZEOF_HEAD_TRANSMSG	(4)
#define	DEF_SECTOR_SIZE			(512)

typedef struct {
	unsigned short uMsgSize;
	unsigned short uMsgID;
	unsigned char  ptrMsg[DEF_SECTOR_SIZE];
} stTRANSMSG;

typedef enum {
	ID_TRANSMSG_FWVERSION = 0,
	ID_TRANSMSG_FRAMESIZE,
	ID_TRANSMSG_FAIL = 0xFFFF,
} enumTRANSMSGID;

typedef struct {
	unsigned char id;
	unsigned char event;
	short x;
	short y;
	short strength;
} stTouchData;

struct ts_event {
	u8  touch_point;
	u16 au16_x[CFG_MAX_TOUCH_POINTS];      // x coordinate
	u16 au16_y[CFG_MAX_TOUCH_POINTS];      // y coordinate
	u8  touch_event[CFG_MAX_TOUCH_POINTS]; // event type
	u8  finger_id[CFG_MAX_TOUCH_POINTS];   // touch ID
	u8  area[CFG_MAX_TOUCH_POINTS];        // finger area
	u16 pressure;
};

struct siw_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct ts_event event;
	int last_touched;
	int lcm_halt;
	u32 addr_ds941;
	u32 addr_ds948;
	int irq_channel;
	int reset_channel;
};

static int siw_i2c_rxdata(struct siw_ts_data *siw_ts,
	char *txdata, int tx_len, char *rxdata, int rx_len)
{
	struct i2c_msg xfer[2];

	xfer[0].addr = siw_ts->client->addr;
	xfer[0].flags = 0;
	xfer[0].len = tx_len;
	xfer[0].buf = txdata;

	xfer[1].addr = siw_ts->client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = rx_len;
	xfer[1].buf = rxdata;

	if (i2c_transfer(siw_ts->client->adapter, xfer, 2) != 2) {
		SIW_TOUCH_ERR("i2c read error\n");
		return -1;
	}

	return rx_len;
}

static int siw_i2c_txdata(struct siw_ts_data *siw_ts, char *txdata, int length)
{
	if (i2c_master_send(siw_ts->client, txdata, length) != length) {
		SIW_TOUCH_ERR("i2c write error\n");
		return -1;
	}
	return length;
}

int siw_firmware_get_version(struct siw_ts_data *siw_ts)
{
	int ret;
	u8 RecTab[70] = {0,};
	u8 SendTab[2] = {IIC_CODE_START, IICCMD_GET_VER};

	siw_i2c_rxdata(siw_ts, SendTab, 2, RecTab, 68);

	SIW_TOUCH_ERR("FW Version: %c%c%c%02d_%02x.%02x_%d.%d.%d_%d.%d.%d\r\n",
		RecTab[8], RecTab[9], RecTab[10], RecTab[11], RecTab[14], RecTab[15],
		RecTab[16], RecTab[17], RecTab[18], RecTab[19], RecTab[20], RecTab[21]);

	return 0;
}

#ifdef SIW_SLOT_A
//read touch point information
static int siw_read_data(struct siw_ts_data *siw_ts)
{
	struct ts_event *event = &siw_ts->event;
	u8 cmd1[2] = {IIC_CODE_START, IICCMD_GET_EVENTNUM};
	u8 cmd2[2] = {IIC_CODE_START, IICCMD_GET_EVENT};
	u8 rcv[82] = {0};
	u8 nFingers = 0;
	stTouchData	*ptrTouchData;
	int ret = -1;
	int cnt;
	int i;
	int current_touched = 0x0;

	ret = siw_i2c_rxdata(siw_ts, cmd1, sizeof(cmd1), rcv, 2);
	if (ret < 0) {
		SIW_TOUCH_DBG("read failed: %d\n", ret);
		return ret;
	}

	nFingers = rcv[0];
	if (nFingers > CFG_MAX_TOUCH_POINTS || !nFingers) {
		SIW_TOUCH_ERR("##ERR, nFinger:%d\n", nFingers);
		event->touch_point = 0;
		return ret;
	}

	cnt = sizeof(stTouchData)*nFingers + 2;
	siw_i2c_rxdata(siw_ts, cmd2, sizeof(cmd2), rcv, cnt);

	ptrTouchData = (stTouchData *)&rcv[2];
	memset(event, 0, sizeof(struct ts_event));

	event->touch_point = nFingers;
	event->pressure = 200;

	for (cnt = 0; cnt < nFingers; cnt++) {
		if (ptrTouchData->id >= CFG_MAX_TOUCH_POINTS) {
			SIW_TOUCH_ERR("##ERR(cnt:%d, nFingers:%d), ID:%d\n",
				cnt, nFingers, ptrTouchData->id);
			event->touch_point = 0;
			return -1;
		}

		event->au16_x[cnt] = ptrTouchData->x;
		event->au16_y[cnt] = ptrTouchData->y;
		event->touch_event[cnt] = ptrTouchData->event;
		event->finger_id[cnt] = ptrTouchData->id;
		event->area[cnt] = ptrTouchData->strength;

		ptrTouchData++;
	}

	for (i = 0; i < event->touch_point; i++) {

		unsigned short x = event->au16_x[i];
		unsigned short y = event->au16_y[i];

		x = x * SCREEN_MAX_X / 32767;
		y = y * SCREEN_MAX_Y / 32767;

		switch (event->touch_event[i]) {
		case TS_EVENT_MOVE:
		case TS_EVENT_PRESS:
			current_touched |= (1 << (event->finger_id[i]));
			input_report_abs(siw_ts->input_dev, ABS_MT_TRACKING_ID, event->finger_id[i]);
			input_report_abs(siw_ts->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(siw_ts->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(siw_ts->input_dev, ABS_MT_TOUCH_MAJOR, event->area[i]);
			input_report_abs(siw_ts->input_dev, ABS_MT_PRESSURE, 200);
			input_mt_sync(siw_ts->input_dev);
			break;
		case TS_EVENT_RELEASE:
			input_report_abs(siw_ts->input_dev, ABS_MT_TRACKING_ID, event->finger_id[i]);
			input_report_abs(siw_ts->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(siw_ts->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(siw_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(siw_ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_sync(siw_ts->input_dev);
			break;
		}
	}

	if (current_touched != siw_ts->last_touched) {
		for (i = 0; i < 16; i++) {
			if ((siw_ts->last_touched & (1 << i))) {
				if ((current_touched & (1 << i)) == 0) {
					input_report_abs(siw_ts->input_dev, ABS_MT_TRACKING_ID, i);
					input_report_abs(siw_ts->input_dev, ABS_MT_POSITION_X, 0);
					input_report_abs(siw_ts->input_dev, ABS_MT_POSITION_Y, 0);
					input_report_abs(siw_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);//ABS_MT_TOUCH_MAJOR=0就会被认为up
					input_report_abs(siw_ts->input_dev, ABS_MT_PRESSURE, 0);
					input_mt_sync(siw_ts->input_dev);
				}
			}
		}
		siw_ts->last_touched = current_touched;
	}
	input_sync(siw_ts->input_dev);
	return 0;
}
#else
static int siw_read_data(struct siw_ts_data *siw_ts)
{
	struct ts_event *event = &siw_ts->event;
	u8 cmd1[2] = {IIC_CODE_START, IICCMD_GET_EVENTNUM};
	u8 cmd2[2] = {IIC_CODE_START, IICCMD_GET_EVENT};
	u8 rcv[82] = {0};
	u8 nFingers = 0;
	stTouchData	*ptrTouchData;
	int ret = -1;
	int cnt;
	int i;

	ret = siw_i2c_rxdata(siw_ts, cmd1, sizeof(cmd1), rcv, 2);
	if (ret < 0) {
		SIW_TOUCH_DBG("read failed: %d\n", ret);
		return ret;
	}

	nFingers = rcv[0];
	if (nFingers > CFG_MAX_TOUCH_POINTS) {
		SIW_TOUCH_ERR("##ERR, nFinger:%d\n", nFingers);
		return ret;
	} else if (nFingers == 0) {
		SIW_TOUCH_DBG("no Finger\n");
	} else {
		cnt = sizeof(stTouchData)*nFingers + 2;
		siw_i2c_rxdata(siw_ts, cmd2, sizeof(cmd2), rcv, cnt);
		ptrTouchData = (stTouchData *)&rcv[2];
	}

	memset(event, 0, sizeof(struct ts_event));
	event->touch_point = nFingers;
	event->pressure = 200;

	for (cnt = 0; cnt < nFingers; cnt++) {
		if (ptrTouchData->id >= CFG_MAX_TOUCH_POINTS) {
			SIW_TOUCH_ERR("##ERR(cnt:%d, nFingers:%d), ID:%d\n",
				cnt, nFingers, ptrTouchData->id);
			return -1;
		}

		event->au16_x[cnt] = ptrTouchData->x;
		event->au16_y[cnt] = ptrTouchData->y;
		event->touch_event[cnt] = ptrTouchData->event;
		event->finger_id[cnt] = ptrTouchData->id;
		event->area[cnt] = ptrTouchData->strength;

		ptrTouchData++;
	}

	for (i = 0; i < event->touch_point; i++) {
		unsigned short x = event->au16_x[i];
		unsigned short y = event->au16_y[i];

		x = x * SCREEN_MAX_X / 32767;
		y = y * SCREEN_MAX_Y / 32767;
		x = 1920 - x;
		y = 1200 - y;

		input_mt_slot(siw_ts->input_dev, event->finger_id[i]);
		if ((event->touch_event[i] == TS_EVENT_MOVE)
				|| (event->touch_event[i] == TS_EVENT_PRESS))
			input_mt_report_slot_state(siw_ts->input_dev, MT_TOOL_FINGER, true);

		input_report_abs(siw_ts->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(siw_ts->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(siw_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(siw_ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
	}

	input_mt_sync_frame(siw_ts->input_dev);
	input_sync(siw_ts->input_dev);
	return 0;
}
#endif

static irqreturn_t siw_ts_interrupt(int irq, void *dev_id)
{
	//SIW_TOUCH_ERR("====== siw_ts TS Interrupt ======\n");
	struct siw_ts_data *siw_ts = (struct siw_ts_data *)dev_id;

	siw_read_data(siw_ts);
	return IRQ_HANDLED;
}

static int siw_hw_reset(struct siw_ts_data *siw_ts)
{
	int rc;

	rc = du90ub941_or_947_enable_i2c_passthrough(siw_ts->client, siw_ts->addr_ds941);
	if (rc)
		return rc;

	rc = du90ub941_or_947_enable_port(siw_ts->client, siw_ts->addr_ds941, DOUBLE_PORT);
	if (rc)
		return rc;

	rc = du90ub948_gpio_input(siw_ts->client, siw_ts->addr_ds948, siw_ts->irq_channel);
	if (rc)
		return rc;

	rc = du90ub941_or_947_gpio_input(siw_ts->client, siw_ts->addr_ds941, siw_ts->irq_channel);
	if (rc)
		return rc;

	rc = du90ub948_gpio_output(siw_ts->client, siw_ts->addr_ds948, siw_ts->reset_channel, 1);
	if (rc)
		return rc;
	msleep(5);

	rc = du90ub948_gpio_output(siw_ts->client, siw_ts->addr_ds948, siw_ts->reset_channel, 0);
	if (rc)
		return rc;
	msleep(10);

	rc = du90ub948_gpio_output(siw_ts->client, siw_ts->addr_ds948, siw_ts->reset_channel, 1);
	if (rc)
		return rc;
	msleep(50);

	return 0;
}

#ifdef SIW_CONFIG_OF
static void siw_parse_dt(struct siw_ts_data *siw_ts)
{
	struct device_node *np = siw_ts->client->dev.of_node;

	of_property_read_u32(np, "addr_ds941", &siw_ts->addr_ds941);
	of_property_read_u32(np, "addr_ds948", &siw_ts->addr_ds948);
	of_property_read_u32(np, "irq_channel", &siw_ts->irq_channel);
	of_property_read_u32(np, "reset_channel", &siw_ts->reset_channel);

	dev_err(&siw_ts->client->dev,
		"addr_ds941=%#x, addr_ds948=%#x, irq_channel=%d, reset_channel=%d\n",
		siw_ts->addr_ds941, siw_ts->addr_ds948,
		siw_ts->irq_channel, siw_ts->reset_channel);
}
#endif

int siw_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct siw_ts_data *siw_ts;
	int err = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c function not supported\n", __func__);
		err = -ENODEV;
		goto err_check_functionality_failed;
	}

	siw_ts = kzalloc(sizeof(struct siw_ts_data), GFP_KERNEL);
	if (!siw_ts) {
		dev_err(&client->dev, "%s: Can't alloc mem\n", __func__);
		err = -ENOMEM;
		goto err_alloc_data_failed;
	}

	siw_ts->client = client;
	i2c_set_clientdata(client, siw_ts);

#ifdef SIW_CONFIG_OF
	if (client->dev.of_node)
		siw_parse_dt(siw_ts);
#endif

	err = siw_hw_reset(siw_ts);
	if (err)
		goto err_i2c_test_failed;

	// test read version
	err = siw_firmware_get_version(siw_ts);
	if (err < 0) {
		printk(KERN_ERR "%s, firmware I2C communication ERROR\n", __func__);
		goto err_i2c_test_failed;
	}

	siw_ts->input_dev = input_allocate_device();
	if (!siw_ts->input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "%s: failed to allocate input device\n", __func__);
		goto err_input_dev_alloc_failed;
	}

	siw_ts->input_dev->phys = "input/ts_main";
	siw_ts->input_dev->name = MODULE_NAME;
	siw_ts->input_dev->id.bustype = BUS_I2C;

#ifdef SIW_SLOT_A
	set_bit(EV_SYN, siw_ts->input_dev->evbit);
	set_bit(EV_KEY, siw_ts->input_dev->evbit);
	set_bit(EV_ABS, siw_ts->input_dev->evbit);
	set_bit(ABS_PRESSURE, siw_ts->input_dev->absbit);
	set_bit(INPUT_PROP_DIRECT, siw_ts->input_dev->propbit);

	input_set_abs_params(siw_ts->input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(siw_ts->input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(siw_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(siw_ts->input_dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);
	input_set_abs_params(siw_ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
#else
	input_set_abs_params(siw_ts->input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(siw_ts->input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(siw_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(siw_ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

	input_mt_init_slots(siw_ts->input_dev, 10, INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	input_set_capability(siw_ts->input_dev, EV_KEY, KEY_LEFTMETA);
#endif

	err = input_register_device(siw_ts->input_dev);
	if (err) {
		dev_err(&client->dev, "%s: failed to register input device: %s\n",
				__func__, dev_name(&client->dev));
		goto err_input_register_device_failed;
	}

	if (client->irq) {
		err = request_threaded_irq(client->irq, NULL, siw_ts_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->dev.driver->name, siw_ts);
		if (err < 0) {
			dev_err(&client->dev, "%s: request irq failed\n", __func__);
			goto err_input_register_device_failed;
		}
	}

	return 0;

err_input_register_device_failed:
	input_free_device(siw_ts->input_dev);
err_input_dev_alloc_failed:
err_i2c_test_failed:
	i2c_set_clientdata(client, NULL);
	kfree(siw_ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return err;
}

int siw_ts_remove(struct i2c_client *client)
{
	struct siw_ts_data *siw_ts;

	siw_ts = i2c_get_clientdata(client);
	input_unregister_device(siw_ts->input_dev);
	i2c_set_clientdata(client, NULL);
	kfree(siw_ts);

	return 0;
}

#ifdef SIW_CONFIG_OF
static const struct of_device_id siw_match_table[] = {
	{.compatible = "siw,sw4101"},
	{},
};
#endif

static const struct i2c_device_id siw_ts_id[] = {
	{MODULE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, siw_ts_id);

static struct i2c_driver siw_ts_driver = {
	.probe			= siw_ts_probe,
	.remove			= siw_ts_remove,
	.id_table		= siw_ts_id,
	.driver	= {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
#ifdef SIW_CONFIG_OF
		.of_match_table = of_match_ptr(siw_match_table),
#endif
	},
};

static int __init siw_ts_init(void)
{
	return i2c_add_driver(&siw_ts_driver);
}

static void __exit siw_ts_exit(void)
{
	i2c_del_driver(&siw_ts_driver);
}

module_init(siw_ts_init);
module_exit(siw_ts_exit);

MODULE_AUTHOR("Silicon Works");
MODULE_DESCRIPTION("Silicon Works TouchScreen Driver");
MODULE_LICENSE("GPL");

