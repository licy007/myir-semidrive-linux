/*
 * ----------------------------------------------------------------------------
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
 * ----------------------------------------------------------------------------
 *
 */

#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/rpmsg.h>

#define MAX_I2C_NUM 16
#define I2C_MSG_HEAD_LEN 6 //sizeof(i2c_msg.addr)+sizeof(i2c_msg.flags)+sizeof(i2c_msg.len)

#define PAYLOAD_HEAD_LEN	sizeof(common_head_t)
#define PAYLOAD_MAX_LEN		112
#define PAYLOAD_DATA_LEN	(PAYLOAD_MAX_LEN - PAYLOAD_HEAD_LEN)

static int print_i2c_msg = -1; //for debug purpose

static struct rpmsg_device *g_rpmsg_device = NULL;

typedef struct __sd_vi2c_t {
	struct platform_device *pdev;
	struct rpmsg_device *rpmsg_device;
	struct i2c_adapter adap;
	uint8_t adap_num;
	struct i2c_msg *msgs;
	int msgs_num;
	struct completion done;
	spinlock_t time_lock;
	uint16_t time_flag;
	struct mutex cb_mutex;
	bool cb_valid;
	uint8_t *r_msg;
	uint8_t *r_msg_p;
	uint16_t r_msg_len;
	uint16_t r_accept_len;
	int8_t trans_stat;
} sd_vi2c_t;

static sd_vi2c_t *sd_vi2c_arr[MAX_I2C_NUM];

typedef struct __common_head_t {
	uint8_t adap_num;
	uint8_t msg_type;
	uint16_t time_flag;
} __packed common_head_t;

typedef struct __head_info_t {
	common_head_t common_head;
	uint16_t msg_len;
	uint16_t msg_num;
} __packed head_info_t;

typedef struct __data_info_t {
	common_head_t common_head;
	char i2c_data[PAYLOAD_DATA_LEN];
} __packed data_info_t;

typedef struct __end_info_t {
	common_head_t common_head;
	int8_t status;
} __packed end_info_t;

typedef struct __payload_t {
	common_head_t common_head;
	char buf[PAYLOAD_DATA_LEN];
} __packed payload_t;

static uint8_t I2C_HEAD = 0;
static uint8_t I2C_DATA = 1;
static uint8_t I2C_END = 2;

/*
 * unpack data receiced from remote and populate to client i2c msg buf
 */
void unpack_data(sd_vi2c_t *sd_vi2c_p, int8_t status)
{
	int i;
	if (sd_vi2c_p->r_msg_len) {
		//copy read back data to client buf
		sd_vi2c_p->r_msg_p = sd_vi2c_p->r_msg;
		for (i = 0; i < sd_vi2c_p->msgs_num; i++) {
			if (sd_vi2c_p->msgs[i].flags & I2C_M_RD) {
				memcpy(sd_vi2c_p->msgs[i].buf, sd_vi2c_p->r_msg_p,
					sd_vi2c_p->msgs[i].len);
				sd_vi2c_p->r_msg_p += sd_vi2c_p->msgs[i].len;
			}
		}
		sd_vi2c_p->trans_stat = status;
	} else {
		// only write
		sd_vi2c_p->trans_stat = status;
	}
}

/*
 * pack local client i2c msg need send to remote
 */
void pack_data(uint8_t adap_num, uint16_t time_flag, uint8_t *t_msg_data, uint16_t t_msg_len,
		int t_msg_num, head_info_t *head, end_info_t *end, uint8_t *t_payload_data)
{
	int i;
	int quotient_size = t_msg_len / PAYLOAD_DATA_LEN;
	int remainder_size = t_msg_len % PAYLOAD_DATA_LEN;
	data_info_t *data = (data_info_t *)t_payload_data;

	for (i = 0; i < quotient_size; i++) {
		data->common_head.adap_num = adap_num;
		data->common_head.msg_type = I2C_DATA;
		data->common_head.time_flag = time_flag;
		memcpy(data->i2c_data, t_msg_data, PAYLOAD_DATA_LEN);
		t_msg_data += PAYLOAD_DATA_LEN;
		data++;
	}

	if (remainder_size) {
		data->common_head.adap_num = adap_num;
		data->common_head.msg_type = I2C_DATA;
		data->common_head.time_flag = time_flag;
		memcpy(data->i2c_data, t_msg_data, remainder_size);
	}

	head->common_head.adap_num = adap_num;
	head->common_head.msg_type = I2C_HEAD;
	head->common_head.time_flag = time_flag;
	head->msg_len = t_msg_len;
	head->msg_num = t_msg_num;

	end->common_head.adap_num = adap_num;
	end->common_head.msg_type = I2C_END;
	end->common_head.time_flag = time_flag;
}

/*
 * send local data to remote
 */
int send_data(struct rpmsg_device *rpmsg_device,
	head_info_t *head, end_info_t *end, uint8_t *t_payload, uint16_t t_payload_len)
{
	int i;
	int quotient_size = t_payload_len / PAYLOAD_MAX_LEN;
	int remainder_size = t_payload_len % PAYLOAD_MAX_LEN;
	data_info_t *data = (data_info_t *)t_payload;

	if (rpmsg_send(rpmsg_device->ept, head, sizeof(head_info_t))) {
		pr_err("adapter[%u], rpmsg_send head fail\n",
			head->common_head.adap_num);
		return -1;
	}

	for (i = 0; i < quotient_size; i++) {
		if (rpmsg_send(rpmsg_device->ept, data, PAYLOAD_MAX_LEN)) {
			pr_err("adapter[%u], rpmsg_send data1 fail\n",
				data->common_head.adap_num);
			return -1;
		}
		data++;
	}

	if (remainder_size) {
		if (rpmsg_send(rpmsg_device->ept, data, remainder_size)) {
			pr_err("adapter[%u], rpmsg_send data2 fail\n",
				data->common_head.adap_num);
			return -1;
		}
	}

	if (rpmsg_send(rpmsg_device->ept, end, sizeof(end_info_t))) {
		pr_err("adapter[%u], rpmsg_send end fail\n",
			end->common_head.adap_num);
		return -1;
	}

	return 0;
}

static int sd_vi2c_xfer(struct i2c_adapter *adap,
	struct i2c_msg msgs[], int num)
{
	int i, j;
	int ret = num;
	uint16_t t_msg_len = 0;
	uint8_t *t_msg_data = NULL;
	uint8_t *t_msg_data_p = NULL;
	uint16_t t_payload_len = 0;
	uint8_t *t_payload_data = NULL;
	head_info_t head_info = {{0},};
	end_info_t end_info = {{0},};
	sd_vi2c_t *sd_vi2c_p = i2c_get_adapdata(adap);

	mutex_lock(&sd_vi2c_p->cb_mutex);

	sd_vi2c_p->msgs = msgs;
	sd_vi2c_p->msgs_num = num;
	sd_vi2c_p->time_flag++;

	if (num <= 0) {
		pr_err("adapter[%u], invalid i2c msgs num\n",
			sd_vi2c_p->adap_num);
		mutex_unlock(&sd_vi2c_p->cb_mutex);
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (msgs[i].flags & I2C_M_RD)
			t_msg_len += I2C_MSG_HEAD_LEN;
		else
			t_msg_len += I2C_MSG_HEAD_LEN + msgs[i].len;
	}

	t_msg_data = kzalloc(t_msg_len, GFP_KERNEL);
	if (!t_msg_data) {
		pr_err("adapter[%u], alloc mem for t_msg_data fail\n",
			sd_vi2c_p->adap_num);
		mutex_unlock(&sd_vi2c_p->cb_mutex);
		return -EINVAL;
	}

	t_msg_data_p = t_msg_data;
	for (i = 0; i < num; i++) {
		memcpy(t_msg_data_p, &msgs[i], I2C_MSG_HEAD_LEN);
		t_msg_data_p += I2C_MSG_HEAD_LEN;
		if (!(msgs[i].flags & I2C_M_RD)) {
			memcpy(t_msg_data_p, msgs[i].buf, msgs[i].len);
			t_msg_data_p += msgs[i].len;
		}
	}

	t_payload_len = t_msg_len +
		(t_msg_len / PAYLOAD_DATA_LEN + !!(t_msg_len % PAYLOAD_DATA_LEN)) * PAYLOAD_HEAD_LEN;

	t_payload_data = kzalloc(t_payload_len, GFP_KERNEL);
	if (!t_payload_data) {
		kfree(t_msg_data);
		pr_err("adapter[%u], alloc mem for t_payload_data fail\n",
			sd_vi2c_p->adap_num);
		mutex_unlock(&sd_vi2c_p->cb_mutex);
		return -EINVAL;
	}

	pack_data(sd_vi2c_p->adap_num, sd_vi2c_p->time_flag, t_msg_data, t_msg_len, num,
		&head_info, &end_info, t_payload_data);

	if (send_data(sd_vi2c_p->rpmsg_device,
			&head_info, &end_info, t_payload_data, t_payload_len)) {
		ret = -EINVAL;
		pr_err("adapter[%u], send data fail\n", sd_vi2c_p->adap_num);
		mutex_unlock(&sd_vi2c_p->cb_mutex);
		goto done;
	}

	sd_vi2c_p->cb_valid = true;

	mutex_unlock(&sd_vi2c_p->cb_mutex);

	if (!wait_for_completion_timeout(&sd_vi2c_p->done, msecs_to_jiffies(3000))) {
		pr_err("adapter[%u], timeout wait for rpmsg callback\n",
			sd_vi2c_p->adap_num);
		ret = -EINVAL;
		goto done;
	}

	ret = sd_vi2c_p->trans_stat;
	if (ret < 0)
		pr_err("adapter[%u], completed with error %d\n", sd_vi2c_p->adap_num,
				sd_vi2c_p->trans_stat);

done:
	if (print_i2c_msg == sd_vi2c_p->adap_num) {
		pr_err("adapter[%u] show payload msg\n", sd_vi2c_p->adap_num);
		for (i = 0; i < t_payload_len; i++)
			pr_err("adapter[%u], payload[%d]=%#x\n",
				sd_vi2c_p->adap_num, i, t_payload_data[i]);

		pr_err("adapter[%u] show i2c msg\n", sd_vi2c_p->adap_num);
		for (i = 0; i < num; i++) {
			pr_err("adapter[%u], addr=%#x, flag=%u, len=%u\n",
				sd_vi2c_p->adap_num, msgs[i].addr, msgs[i].flags, msgs[i].len);
			for (j = 0; j < msgs[i].len; j++)
				pr_err("adapter[%u], i2c_msg[%d]=%#x\n",
					sd_vi2c_p->adap_num, j, msgs[i].buf[j]);
		}
	}

	kfree(t_msg_data);
	kfree(t_payload_data);
	mutex_lock(&sd_vi2c_p->cb_mutex);
	sd_vi2c_p->cb_valid = false;
	mutex_unlock(&sd_vi2c_p->cb_mutex);
	return ret;
}

u32 sd_vi2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_10BIT_ADDR |
		I2C_FUNC_I2C |
		I2C_FUNC_SMBUS_BYTE |
		I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA |
		I2C_FUNC_SMBUS_BLOCK_DATA |
		I2C_FUNC_SMBUS_I2C_BLOCK;
}

static const struct i2c_algorithm sd_vi2c_algo = {
	.master_xfer = sd_vi2c_xfer,
	.functionality = sd_vi2c_func,
};

#ifdef CONFIG_OF
static const struct of_device_id sd_vi2c_of_match[] = {
	{.compatible = "sd,virtual-i2c"},
	{},
};
MODULE_DEVICE_TABLE(of, sd_vi2c_of_match);
#endif

static ssize_t vi2c_enable_msg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val, ret;

	ret = kstrtoint(buf, 10, &val);
	if (ret)
		return ret;

	print_i2c_msg = val;
	pr_err("%s, print_i2c_msg=%d\n", __func__, print_i2c_msg);
	return count;
}

static DEVICE_ATTR(show_msg, 0664, NULL, vi2c_enable_msg);

static struct attribute *vi2c_attributes[] = {
	&dev_attr_show_msg.attr,
	NULL,
};

static const struct attribute_group vi2c_attr_group = {
	.attrs = vi2c_attributes,
};

static int sd_vi2c_plat_probe(struct platform_device *pdev)
{
	int ret;
	uint8_t adap_num;
	sd_vi2c_t *sd_vi2c_p;

	if (!g_rpmsg_device) {
		pr_err("%s, vi2c no rpmsg device\n", __func__);
		return -ENODEV;
	}

	ret = device_property_read_u8(&pdev->dev, "phy-num", &adap_num);
	if (ret)
		return ret;

	pr_err("%s, vi2c phy-num=%u\n", __func__, adap_num);

	sd_vi2c_p =
		sd_vi2c_arr[adap_num] = kzalloc(sizeof(sd_vi2c_t), GFP_KERNEL);
	sd_vi2c_p->pdev = pdev;
	sd_vi2c_p->rpmsg_device = g_rpmsg_device;
	sd_vi2c_p->adap_num = adap_num;
	snprintf(sd_vi2c_p->adap.name,
		sizeof(sd_vi2c_p->adap.name), "sd virtual i2c adapter");
	sd_vi2c_p->adap.owner = THIS_MODULE;
	sd_vi2c_p->adap.class = I2C_CLASS_DEPRECATED;
	sd_vi2c_p->adap.dev.of_node = sd_vi2c_p->pdev->dev.of_node;
	sd_vi2c_p->adap.dev.parent = &sd_vi2c_p->pdev->dev;
	sd_vi2c_p->adap.retries = 3;
	sd_vi2c_p->adap.nr = adap_num + 16;
	sd_vi2c_p->adap.algo = &sd_vi2c_algo;

	init_completion(&sd_vi2c_p->done);
	spin_lock_init(&sd_vi2c_p->time_lock);
	mutex_init(&sd_vi2c_p->cb_mutex);
	i2c_set_adapdata(&sd_vi2c_p->adap, sd_vi2c_p);
	platform_set_drvdata(pdev, sd_vi2c_p);

	ret = i2c_add_numbered_adapter(&sd_vi2c_p->adap);
	if (ret) {
		kfree(sd_vi2c_p);
		pr_err("%s, dad i2c adapter[%u] fail\n", __func__, adap_num);
		return -ENODEV;
	}

	ret = sysfs_create_group(&sd_vi2c_p->pdev->dev.kobj, &vi2c_attr_group);
	if (ret)
		pr_err("%s, adapter[%u] create sysfs file fail\n",
			__func__, adap_num);

	return 0;
}

static int sd_vi2c_plat_remove(struct platform_device *pdev)
{
	sd_vi2c_t *sd_vi2c_p = platform_get_drvdata(pdev);

	sysfs_remove_group(&sd_vi2c_p->pdev->dev.kobj, &vi2c_attr_group);
	kfree(sd_vi2c_p);
	return 0;
}

static struct platform_driver sd_vi2c_driver = {
	.probe = sd_vi2c_plat_probe,
	.remove = sd_vi2c_plat_remove,
	.driver	= {
		.name = "sd-virtual-i2c",
		.of_match_table = of_match_ptr(sd_vi2c_of_match),
	},
};

static struct rpmsg_device_id rpmsg_vi2c_id_table[] = {
	{.name = "safety,vi2c"},
	{},
};

static int rpmsg_vi2c_cb(struct rpmsg_device *rpdev,
			void *data, int len, void *priv, u32 src)
{
	payload_t *payload = (payload_t *)data;
	uint8_t adap_num = payload->common_head.adap_num;
	uint8_t msg_type = payload->common_head.msg_type;
	uint16_t time_flag = payload->common_head.time_flag;

	mutex_lock(&sd_vi2c_arr[adap_num]->cb_mutex);

	if (!sd_vi2c_arr[adap_num]->cb_valid) {
		sd_vi2c_arr[adap_num]->trans_stat = -20;
		goto done;
	}

	if (len < PAYLOAD_HEAD_LEN) {
		sd_vi2c_arr[adap_num]->trans_stat = -21;
		complete(&sd_vi2c_arr[adap_num]->done);
		goto done;
	}

	if (len > PAYLOAD_MAX_LEN) {
		sd_vi2c_arr[adap_num]->trans_stat = -22;
		complete(&sd_vi2c_arr[adap_num]->done);
		goto done;
	}

	if (adap_num > 15 || msg_type > I2C_END) {
		sd_vi2c_arr[adap_num]->trans_stat = -23;
		complete(&sd_vi2c_arr[adap_num]->done);
		goto done;
	}

	if (time_flag != sd_vi2c_arr[adap_num]->time_flag) {
		sd_vi2c_arr[adap_num]->trans_stat = -24;
		complete(&sd_vi2c_arr[adap_num]->done);
		goto done;
	}

	if (msg_type == I2C_HEAD) {
		head_info_t *r_head = (head_info_t *)payload;

		if (sd_vi2c_arr[adap_num]->r_msg)
			kfree(sd_vi2c_arr[adap_num]->r_msg);

		sd_vi2c_arr[adap_num]->r_accept_len = 0;
		sd_vi2c_arr[adap_num]->r_msg_len = r_head->msg_len;
		sd_vi2c_arr[adap_num]->r_msg =
			kzalloc(sd_vi2c_arr[adap_num]->r_msg_len, GFP_KERNEL);
		sd_vi2c_arr[adap_num]->r_msg_p = sd_vi2c_arr[adap_num]->r_msg;

	} else if (msg_type == I2C_DATA) {
		data_info_t *r_data = (data_info_t *)payload;

		if (sd_vi2c_arr[adap_num]->r_msg) {

			len = len - PAYLOAD_HEAD_LEN;
			sd_vi2c_arr[adap_num]->r_accept_len += len;

			if (sd_vi2c_arr[adap_num]->r_accept_len > sd_vi2c_arr[adap_num]->r_msg_len) {
				sd_vi2c_arr[adap_num]->trans_stat = -13;
				complete(&sd_vi2c_arr[adap_num]->done);
				goto done;
			}

			memcpy(sd_vi2c_arr[adap_num]->r_msg_p, r_data->i2c_data, len);
			sd_vi2c_arr[adap_num]->r_msg_p += len;
		}
	} else if (msg_type == I2C_END) {
		end_info_t *r_end = (end_info_t *)payload;
		uint16_t len = sd_vi2c_arr[adap_num]->r_msg_p - sd_vi2c_arr[adap_num]->r_msg;

		if (sd_vi2c_arr[adap_num]->r_msg && (len == sd_vi2c_arr[adap_num]->r_msg_len)) {
			unpack_data(sd_vi2c_arr[adap_num], r_end->status);
		} else {
			pr_err("%s, adapter[%u] err: len=%u, r_msg=%p, r_msg_len=%u\n",
				__func__, adap_num, len,
				sd_vi2c_arr[adap_num]->r_msg, sd_vi2c_arr[adap_num]->r_msg_len);
		}

		if (sd_vi2c_arr[adap_num]->r_msg)
			kfree(sd_vi2c_arr[adap_num]->r_msg);
		sd_vi2c_arr[adap_num]->r_msg = NULL;
		sd_vi2c_arr[adap_num]->r_msg_p = NULL;
		sd_vi2c_arr[adap_num]->r_msg_len = 0;
		sd_vi2c_arr[adap_num]->r_accept_len = 0;

		complete(&sd_vi2c_arr[adap_num]->done);
	}

done:
	mutex_unlock(&sd_vi2c_arr[adap_num]->cb_mutex);
	return 0;
}

static int rpmsg_vi2c_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_channel_info chinfo;

	if (!rpdev) {
		pr_err("%s, err: vi2c rpmsg rpdev\n", __func__);
		return -ENODEV;
	}

	strncpy(chinfo.name, "vi2c", RPMSG_NAME_SIZE);
	chinfo.src = 73;
	chinfo.dst = RPMSG_ADDR_ANY;

	rpdev->ept = rpmsg_create_ept(rpdev, rpmsg_vi2c_cb, rpdev, chinfo);
	if (!rpdev->ept) {
		pr_err("%s, err: vi2c rpmsg chanel\n", __func__);
		return -ENODEV;
	}

	g_rpmsg_device = rpdev;

	return 0;
}

static void rpmsg_vi2c_remove(struct rpmsg_device *rpdev)
{
	rpmsg_destroy_ept(rpdev->ept);
	g_rpmsg_device = NULL;
}

static struct rpmsg_driver rpmsg_vi2c_driver = {
	.probe = rpmsg_vi2c_probe,
	.remove = rpmsg_vi2c_remove,
	.id_table = rpmsg_vi2c_id_table,
	.drv = {
		.name = "rpmsg-vi2c-driver",
	},
};

static int __init sd_vi2c_init_driver(void)
{
	int ret;

	ret = register_rpmsg_driver(&rpmsg_vi2c_driver);
	if (ret < 0) {
		pr_err("%s, failed to register sd rpmsg driver\n", __func__);
		return ret;
	}

	ret = platform_driver_register(&sd_vi2c_driver);
	if (ret) {
		pr_err("%s, failed to register sd vi2c driver\n", __func__);
		unregister_rpmsg_driver(&rpmsg_vi2c_driver);
	}

	return ret;
}
subsys_initcall(sd_vi2c_init_driver);

static void __exit sd_vi2c_exit_driver(void)
{
	unregister_rpmsg_driver(&rpmsg_vi2c_driver);
	platform_driver_unregister(&sd_vi2c_driver);
}
module_exit(sd_vi2c_exit_driver);

MODULE_AUTHOR("davy@semidrive");
MODULE_DESCRIPTION("semidrive virtual i2c controller driver");
MODULE_LICENSE("GPL v2");
