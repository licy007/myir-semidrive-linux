// SPDX-License-Identifier: GPL-2.0+
/**
 *  Driver for Marvell Automotive switch chips
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>

struct mv_data {
	unsigned int portaddr;
	unsigned int regnum;
	unsigned int is_write;
	unsigned int data[5];
};

#define MV_GET_CMD				_IOWR('M', 8, struct mv_data)
#define MV_SET_TEST_MODE			_IOW('M', 9, struct mv_data)
#define MV_SET_AUTO_NEGOTIATION			_IOW('M', 10, struct mv_data)
#define MV_GET_AUTO_NEGOTIATION_STATE		_IOWR('M', 11, struct mv_data)
#define MV_ACCESS_PHY_CMD			_IOWR('M', 12, struct mv_data)
#define MV_ACCESS_LBU_CMD			_IOWR('M', 13, struct mv_data)

#define MV_5050_PHYID				0x50505050
#define MV_5072_PHYID				0x50725072
#define MV_GLOBAL1_PORT				0x1b
#define MV_GLOBAL2_PORT				0x1c

#define MV_INTERNAL_PHY_TEST_ADDR		0x1
#define MV_EXTERNAL_PHY_TEST_ADDR		0x1
#define MV_INTERNAL_PHY_TEST_CTRL_REG		0x836
#define MV_EXTERNAL_PHY_TEST_CTRL_REG		0x904
#define MV_PHY_TEST_CTRL_OFFSET			13

#define MV_PHY_AUTO_NEGO_CTRL_REG		0x200
#define MV_PHY_AUTO_NEGO_EN_OFFSET		12

#define MV_PHY_AUTO_NEGO_STA_REG		0x201

#define MV_SMI_PHY_CMD_REG			0x18
#define MV_SMI_BUSY_OFFSET			15
#define MV_SMI_FUNC_OFFSET			13
#define MV_SMI_OP_OFFSET			10
#define MV_SMI_DEV_ADDR_OFFSET			5
#define MV_SMI_REG_ADDR_OFFSET			0

#define MV_SMI_PHY_DATA_REG			0x19

#define MV_SMI_OP_C45_WRITE_ADDR		0
#define MV_SMI_OP_C45_WRITE_DATA		1
#define MV_SMI_OP_C45_READ_WITH_INC		2
#define MV_SMI_OP_C45_READ			3

#define MV_INTERNAL_PHY				0
#define MV_EXTERNAL_PHY				1
#define MV_5050_MAX_INTERNA_PHY			5
#define MV_5072_MAX_INTERNA_PHY			7
#define MV_5050_EXTERNA_PHY_OFFSET		5
#define MV_5072_EXTERNA_PHY_OFFSET		8

#define MV_AHB_MASTER				0x11
#define MV_AHB_MASTER_BUSY			15
#define MV_AHB_MASTER_POINTER			8
#define MV_AHB_LBU_BUSY				7

#define MV_AHB_MASTER_CTRL			0x00
#define MV_AHB_TRANS_MODE			0x02
#define MV_AHB_TRANS_ADDR0			0x04
#define MV_AHB_TRANS_ADDR1			0x05
#define MV_AHB_TRANS_ADDR2			0x06
#define MV_AHB_TRANS_ADDR3			0x07
#define MV_AHB_TRANS_DATA0			0x10
#define MV_AHB_TRANS_DATA1			0x11
#define MV_AHB_TRANS_DATA2			0x12
#define MV_AHB_TRANS_DATA3			0x13

#define MV_AHB_WRITE_START			0x81
#define MV_AHB_READ_START			0x80

#define MV_5050_INTERNAL_PHY_DEVICE1		1
#define MV_5050_INTERNAL_PHY_PMA_PMD_REG	0x0834
#define MV_5050_INTERNAL_PHY_PMA_PMD_DATA	0x8000

#define MV_5050_INTERNAL_PHY_DEVICE3		3
#define MV_5050_INTERNAL_PHY_COPPER_CTRL_REG	0x8000
#define MV_5050_INTERNAL_PHY_COPPER_CTRL_DATA	0x0200

#define MV_G9X_REF_5050_MDIO_ADDR		0x1f
#define MV_5050_PORT8				0x8
#define MV_5050_PORT8_PHY_CTRL_REG		0x1
#define MV_5050_PORT8_PHY_CTRL_DATA		0xe03e

#define MV_5072_INTERNAL_PHY_DEVICE3		3
#define MV_5072_INTERNAL_PHY_COPPER_CTRL_REG	0x8000
#define MV_5072_INTERNAL_PHY_COPPER_CTRL_DATA	0x0200

static struct phy_device *mv_phy[2];
static const struct file_operations mv_5050_fops;
static const struct file_operations mv_5072_fops;

static inline int mv_read(struct phy_device *phydev, u32 portaddr, u32 regnum)
{
	return mdiobus_read(phydev->mdio.bus, portaddr, regnum);
}

static inline int mv_write(struct phy_device *phydev, u32 portaddr, u32 regnum, u16 val)
{
	return mdiobus_write(phydev->mdio.bus, portaddr, regnum, val);
}

static unsigned int mv_get_count(struct phy_device *phy, int port, int num)
{
	unsigned int data;

	while (mv_read(phy, MV_GLOBAL1_PORT, 0x1d) & (1<<15))
		udelay(3);

	data = 0xc000 | ((port) << 5) | (num);
	mv_write(phy, MV_GLOBAL1_PORT, 0x1d, data);

	while (mv_read(phy, MV_GLOBAL1_PORT, 0x1d) & (1<<15))
		udelay(3);

	return mv_read(phy, MV_GLOBAL1_PORT, 0x1e) << 16 | mv_read(phy, MV_GLOBAL1_PORT, 0x1f);
}

static int mv_get_data(struct phy_device *phy, struct mv_data *data)
{
	if (data->is_write && data->regnum != 0xa11) {
		mv_write(phy, data->portaddr, data->regnum, (u16)(data->data[0]));
	} else {
		if (data->regnum == 0xa11) {
			data->data[0] = mv_get_count(phy, data->portaddr, 0);
			data->data[1] = mv_get_count(phy, data->portaddr, 1);
			data->data[2] = mv_get_count(phy, data->portaddr, 2);
			data->data[3] = mv_get_count(phy, data->portaddr, 0xe);
			data->data[4] = mv_get_count(phy, data->portaddr, 0xf);
		} else
			data->data[0] = mv_read(phy, data->portaddr, data->regnum);
	}
	return 0;
}

static int mv_wait_smi_idle(struct phy_device *phy, int timeout)
{
	u16 smi_phy_reg;

	do {
		udelay(1);
		smi_phy_reg = mv_read(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_CMD_REG);
	} while (--timeout && (smi_phy_reg & (1 << MV_SMI_BUSY_OFFSET)));

	if (!timeout) {
		phydev_err(phy, "mv_switch: wait smi idle out of time\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int mv_write_phy_register(struct phy_device *phy, u16 func,
	u16 smi_device, u16 smi_register, u16 reg_addr, u16 write_data)
{
	u16 base_cmd;

	if (mv_wait_smi_idle(phy, 50) < 0)
		return -EBUSY;

	base_cmd = (1 << MV_SMI_BUSY_OFFSET) | (func << MV_SMI_FUNC_OFFSET) | \
		(smi_device << MV_SMI_DEV_ADDR_OFFSET) | smi_register;

	mv_write(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_DATA_REG, reg_addr);
	mv_write(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_CMD_REG, \
		base_cmd | (MV_SMI_OP_C45_WRITE_ADDR << MV_SMI_OP_OFFSET));

	if (mv_wait_smi_idle(phy, 50) < 0)
		return -EBUSY;

	mv_write(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_DATA_REG, write_data);
	mv_write(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_CMD_REG, \
		base_cmd | (MV_SMI_OP_C45_WRITE_DATA << MV_SMI_OP_OFFSET));

	if (mv_wait_smi_idle(phy, 50) < 0)
		return -EBUSY;

	return 0;
}

static int mv_read_phy_register(struct phy_device *phy, u16 func,
			u16 smi_device, u16 smi_register, u16 reg_addr)
{
	u16 base_cmd;

	if (mv_wait_smi_idle(phy, 50) < 0)
		return -EBUSY;

	base_cmd = (1 << MV_SMI_BUSY_OFFSET) | (func << MV_SMI_FUNC_OFFSET) | \
		   (smi_device << MV_SMI_DEV_ADDR_OFFSET) | smi_register;

	mv_write(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_DATA_REG, reg_addr);
	mv_write(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_CMD_REG, \
		base_cmd | (MV_SMI_OP_C45_WRITE_ADDR << MV_SMI_OP_OFFSET));

	if (mv_wait_smi_idle(phy, 50) < 0)
		return -EBUSY;

	mv_write(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_CMD_REG, \
		base_cmd | (MV_SMI_OP_C45_READ << MV_SMI_OP_OFFSET));

	if (mv_wait_smi_idle(phy, 50) < 0)
		return -EBUSY;

	return mv_read(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_DATA_REG);
}

static int mv_access_phy(struct phy_device *phy, struct mv_data *data)
{
	if (data->is_write)
		mv_write_phy_register(phy, (u16)(data->data[0]), data->portaddr,
					(u16)(data->data[1]), data->regnum,
					(u16)(data->data[2]));
	else
		data->data[0] = mv_read_phy_register(phy, (u16)(data->data[0]),
					data->portaddr, (u16)(data->data[1]),
					data->regnum);
	return 0;
}

static int mv_wait_ahb_idle(struct phy_device *phy, int timeout)
{
	u16 reg;

	do {
		udelay(1);
		reg = mv_read(phy, MV_GLOBAL2_PORT, MV_AHB_MASTER);
	} while (--timeout && (reg & (1 << MV_AHB_MASTER_BUSY)));

	if (!timeout) {
		phydev_err(phy, "wait AHB idle timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int mv_write_ahb_reg(struct phy_device *phy, u8 addr, u8 data)
{
	u16 tmp = 0;

	tmp = (1 << MV_AHB_MASTER_BUSY) | (addr << MV_AHB_MASTER_POINTER) | data;
	mv_write(phy, MV_GLOBAL2_PORT, MV_AHB_MASTER, tmp);

	return mv_wait_ahb_idle(phy, 50);
}

static int mv_read_ahb_reg(struct phy_device *phy, u8 addr, u8 *data)
{
	u16 tmp = 0;

	tmp = (addr << MV_AHB_MASTER_POINTER);
	mv_write(phy, MV_GLOBAL2_PORT, MV_AHB_MASTER, tmp);

	tmp = mv_read(phy, MV_GLOBAL2_PORT, MV_AHB_MASTER);
	*data = (u8)(tmp & 0xff);
	return 0;
}

static int mv_wait_lbu_idle(struct phy_device *phy, int timeout)
{
	u8 tmp;

	do {
		udelay(1);
		mv_read_ahb_reg(phy, MV_AHB_MASTER_CTRL, &tmp);
	} while (--timeout && (tmp & (1 << MV_AHB_LBU_BUSY)));

	if (!timeout) {
		phydev_err(phy, "wait AHB lbu timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int mv_write_lbu_reg(struct phy_device *phy, u32 addr, u32 data)
{
	mv_write_ahb_reg(phy, MV_AHB_TRANS_ADDR0, (u8)(addr & 0xff));
	mv_write_ahb_reg(phy, MV_AHB_TRANS_ADDR1, (u8)((addr >> 8) & 0xff));
	mv_write_ahb_reg(phy, MV_AHB_TRANS_ADDR2, (u8)((addr >> 16) & 0xff));
	mv_write_ahb_reg(phy, MV_AHB_TRANS_ADDR3, (u8)((addr >> 24) & 0xff));

	mv_write_ahb_reg(phy, MV_AHB_TRANS_DATA0, (u8)(data & 0xff));
	mv_write_ahb_reg(phy, MV_AHB_TRANS_DATA1, (u8)((data >> 8) & 0xff));
	mv_write_ahb_reg(phy, MV_AHB_TRANS_DATA2, (u8)((data >> 16) & 0xff));
	mv_write_ahb_reg(phy, MV_AHB_TRANS_DATA3, (u8)((data >> 24) & 0xff));

	mv_write_ahb_reg(phy, MV_AHB_MASTER_CTRL, MV_AHB_WRITE_START);

	return mv_wait_lbu_idle(phy, 50);
}

static int mv_read_lbu_reg(struct phy_device *phy, u32 addr, u32 *data)
{
	u8 tmp[4];
	int ret = 0;

	mv_write_ahb_reg(phy, MV_AHB_TRANS_ADDR0, (u8)(addr & 0xff));
	mv_write_ahb_reg(phy, MV_AHB_TRANS_ADDR1, (u8)((addr >> 8) & 0xff));
	mv_write_ahb_reg(phy, MV_AHB_TRANS_ADDR2, (u8)((addr >> 16) & 0xff));
	mv_write_ahb_reg(phy, MV_AHB_TRANS_ADDR3, (u8)((addr >> 24) & 0xff));

	mv_write_ahb_reg(phy, MV_AHB_MASTER_CTRL, MV_AHB_READ_START);
	ret = mv_wait_lbu_idle(phy, 50);
	if (ret)
		return ret;

	mv_read_ahb_reg(phy, MV_AHB_TRANS_DATA0, &tmp[0]);
	mv_read_ahb_reg(phy, MV_AHB_TRANS_DATA1, &tmp[1]);
	mv_read_ahb_reg(phy, MV_AHB_TRANS_DATA2, &tmp[2]);
	mv_read_ahb_reg(phy, MV_AHB_TRANS_DATA3, &tmp[3]);

	*data = (tmp[3] << 24) | (tmp[2] << 16) | (tmp[1] << 8) | tmp[0];
	return 0;
}

static int mv_access_lbu(struct phy_device *phy, struct mv_data *data)
{
	int ret = 0;

	if (data->is_write)
		ret = mv_write_lbu_reg(phy, data->data[0], data->data[1]);
	else
		ret = mv_read_lbu_reg(phy, data->data[0], &data->data[1]);
	return ret;
}

static int mv_set_test_mode(struct phy_device *phy, struct mv_data *data)
{
	u16 test_mode;
	u16 phy_type, phy_port, phy_addr, phy_reg, max_internal_port;
	u16 phy_external_port_offset;

	test_mode = (u16)data->data[0];

	if (phy->phy_id == MV_5050_PHYID) {
		max_internal_port = MV_5050_MAX_INTERNA_PHY;
		phy_external_port_offset = MV_5050_EXTERNA_PHY_OFFSET;
	} else {
		max_internal_port = MV_5072_MAX_INTERNA_PHY;
		phy_external_port_offset = MV_5072_EXTERNA_PHY_OFFSET;
	}

	if (((data->portaddr <= max_internal_port) && (test_mode == 3 ||
		test_mode > 5)) || ((data->portaddr > max_internal_port) &&
		(test_mode == 3 || test_mode > 7))) {
		phydev_err(phy, "mv_switch: test_mode set error:%d\n", test_mode);
		return -EINVAL;
	}

	if (data->portaddr <= max_internal_port) {
		phy_type = MV_INTERNAL_PHY;
		phy_port = data->portaddr;
		phy_addr = MV_INTERNAL_PHY_TEST_ADDR;
		phy_reg = MV_INTERNAL_PHY_TEST_CTRL_REG;
	} else {
		phy_type = MV_EXTERNAL_PHY;
		/*
		 * the external phy port addr is only 1 or 2
		 * 5072 port 9 is 1, port 10 is 2
		 * 5050 port 6 is 1, port 7 is 2
		 */
		phy_port = data->portaddr - phy_external_port_offset;
		phy_addr = MV_EXTERNAL_PHY_TEST_ADDR;
		phy_reg = MV_EXTERNAL_PHY_TEST_CTRL_REG;
	}

	return mv_write_phy_register(phy, phy_type, phy_port, phy_addr, \
			phy_reg, (u16)(test_mode << MV_PHY_TEST_CTRL_OFFSET));
}

static int mv_set_auto_negotiation(struct phy_device *phy, struct mv_data *data)
{
	return mv_write_phy_register(phy, MV_INTERNAL_PHY, data->portaddr, 7, \
		MV_PHY_AUTO_NEGO_CTRL_REG, (u16)(1 << MV_PHY_AUTO_NEGO_EN_OFFSET));
}

static int mv_get_auto_negotiation_state(struct phy_device *phy, struct mv_data *data)
{
	return mv_read_phy_register(phy, MV_INTERNAL_PHY, data->portaddr, 7, \
		MV_PHY_AUTO_NEGO_STA_REG);
}

static int mv_write_tx_phy_reg(struct phy_device *phy, u16 data1, u16 data2)
{
	if (mv_wait_smi_idle(phy, 200) < 0)
		return -EBUSY;

	mv_write(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_DATA_REG, data2);
	mv_write(phy, MV_GLOBAL2_PORT, MV_SMI_PHY_CMD_REG, data1);
	return 0;
}

static void mv_5050_disable_tx_EEE_and_energy_detect(struct phy_device *phy)
{
	mv_write_tx_phy_reg(phy, 0x94dd, 0x001b);
	mv_write_tx_phy_reg(phy, 0x94de, 0x0e02);
	mv_write_tx_phy_reg(phy, 0x94dc, 0x0c03);
	mv_write_tx_phy_reg(phy, 0x94ce, 0x003c);
	mv_write_tx_phy_reg(phy, 0x94cd, 0x0007);
	mv_write_tx_phy_reg(phy, 0x94cd, 0x4007);
	mv_write_tx_phy_reg(phy, 0x94ce, 0x0000);
	mv_write_tx_phy_reg(phy, 0x94c0, 0xb100);
	mv_write_tx_phy_reg(phy, 0x94d6, 0x0001);
	mv_write_tx_phy_reg(phy, 0x94d0, 0x1108);
}

static int mv_5050_init(struct phy_device *phy)
{
	int port;

	/* init all 5050 internal T1 phy as slave mode */
	for (port = 1; port <= MV_5050_MAX_INTERNA_PHY; port++)
		mv_write_phy_register(phy, MV_INTERNAL_PHY, port,
			MV_5050_INTERNAL_PHY_DEVICE1, MV_5050_INTERNAL_PHY_PMA_PMD_REG,
			MV_5050_INTERNAL_PHY_PMA_PMD_DATA);
	return 0;
}

static void mv_5050_auto_polarity_reversed(struct phy_device *phy)
{
	int port;

	/*enable all ports's polarity can auto reversed*/
	for (port = 1; port <= MV_5050_MAX_INTERNA_PHY; port++)
		mv_write_phy_register(phy, MV_INTERNAL_PHY, port,
			MV_5050_INTERNAL_PHY_DEVICE3,
			MV_5050_INTERNAL_PHY_COPPER_CTRL_REG,
			MV_5050_INTERNAL_PHY_COPPER_CTRL_DATA);
}

static void mv_5072_auto_polarity_reversed(struct phy_device *phy)
{
	int port;

	/*enable all ports's polarity can auto reversed*/
	for (port = 1; port <= MV_5072_MAX_INTERNA_PHY; port++)
		mv_write_phy_register(phy, MV_INTERNAL_PHY, port,
			MV_5072_INTERNAL_PHY_DEVICE3,
			MV_5072_INTERNAL_PHY_COPPER_CTRL_REG,
			MV_5072_INTERNAL_PHY_COPPER_CTRL_DATA);
}

static long mv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;

	struct phy_device *phy = file->private_data;
	struct mv_data data;

	ret = copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;
	switch (cmd) {
	case MV_GET_CMD:
		ret = mv_get_data(phy, &data);
		break;
	case MV_ACCESS_PHY_CMD:
		ret = mv_access_phy(phy, &data);
		break;
	case MV_ACCESS_LBU_CMD:
		ret = mv_access_lbu(phy, &data);
		break;
	case MV_SET_TEST_MODE:
		ret = mv_set_test_mode(phy, &data);
		break;
	case MV_SET_AUTO_NEGOTIATION:
		ret = mv_set_auto_negotiation(phy, &data);
		break;
	case MV_GET_AUTO_NEGOTIATION_STATE:
		ret = mv_get_auto_negotiation_state(phy, &data);
		break;
	default:
		return -EINVAL;
	}
	ret = copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;
	return ret;
}

static int mv_open(struct inode *inode, struct file *file)
{
	if (file->f_op == &mv_5050_fops)
		file->private_data = mv_phy[0];
	else
		file->private_data = mv_phy[1];
	return 0;
}

static const struct file_operations mv_5050_fops = {
	.owner = THIS_MODULE,
	.open = mv_open,
	.unlocked_ioctl = mv_ioctl,
};

static const struct file_operations mv_5072_fops = {
	.owner = THIS_MODULE,
	.open = mv_open,
	.unlocked_ioctl = mv_ioctl,
};

static struct miscdevice mv_5050 = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mv5050",
	.fops = &mv_5050_fops,
};

static struct miscdevice mv_5072 = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mv5072",
	.fops = &mv_5072_fops,
};

static int mv_probe(struct phy_device *phydev)
{
	struct miscdevice *mv_misc;
	int ret;

	if (phydev->phy_id == MV_5050_PHYID) {
		mv_misc = &mv_5050;
		mv_phy[0] = phydev;
		if (phydev->mdio.addr == MV_G9X_REF_5050_MDIO_ADDR)
			mv_write(phydev, MV_5050_PORT8, MV_5050_PORT8_PHY_CTRL_REG,
					MV_5050_PORT8_PHY_CTRL_DATA);
		else
			mv_5050_init(phydev);
		mv_5050_auto_polarity_reversed(phydev);
		mv_5050_disable_tx_EEE_and_energy_detect(phydev);
	} else {
		mv_misc = &mv_5072;
		mv_phy[1] = phydev;
		mv_5072_auto_polarity_reversed(phydev);
	}

	ret = misc_register(mv_misc);
	if (unlikely(ret)) {
		printk("\033[32m%s register mv switch fail\n", __func__);
		return -1;
	}

	printk("%s register %s successfully.\n", __func__, mv_misc->name);
	return 0;
}

static int mv_soft_reset(struct phy_device *phydev)
{
	return 0;
}

static int mv_config_init(struct phy_device *phydev)
{
	return 0;
}

static int mv_config_aneg(struct phy_device *phydev)
{
	phydev->supported = SUPPORTED_100baseT_Full | SUPPORTED_1000baseT_Full;
	phydev->advertising = SUPPORTED_100baseT_Full | SUPPORTED_1000baseT_Full;
	return 0;
}

static int mv_aneg_done(struct phy_device *phydev)
{
	return 1;
}

static int mv_read_status(struct phy_device *phydev)
{
	phydev->speed = SPEED_1000;
	phydev->link = 1;
	phydev->duplex = DUPLEX_FULL;
	phydev->pause = 0;
	phydev->asym_pause = 0;
	return 0;
}

static struct phy_driver marvellauto_driver[] = { {
	.phy_id		= MV_5050_PHYID,
	.name		= "mv5050",
	.phy_id_mask	= 0xfffffff0,
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_1000baseT_Full),
	.flags		= PHY_POLL,
	.soft_reset	= mv_soft_reset,
	.probe		= mv_probe,
	.config_init	= mv_config_init,
	.config_aneg	= mv_config_aneg,
	.aneg_done	= mv_aneg_done,
	.read_status	= mv_read_status,
}, {
	.phy_id		= MV_5072_PHYID,
	.name		= "mv5072",
	.phy_id_mask	= 0xfffffff0,
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_1000baseT_Full),
	.flags		= PHY_POLL,
	.soft_reset	= mv_soft_reset,
	.probe		= mv_probe,
	.config_init	= mv_config_init,
	.config_aneg	= mv_config_aneg,
	.aneg_done	= mv_aneg_done,
	.read_status	= mv_read_status,
}
};

module_phy_driver(marvellauto_driver);

static struct mdio_device_id __maybe_unused marvellauto_tbl[] = {
	{ MV_5050_PHYID, 0xfffffff0 },
	{ MV_5072_PHYID, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, marvellauto_tbl);

MODULE_DESCRIPTION("Marvell Automotive switch driver");
MODULE_AUTHOR("Dejin Zheng");
MODULE_LICENSE("GPL");
