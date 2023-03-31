// SPDX-License-Identifier: GPL-2.0+
/**
 *  Driver for Marvell Automotive 88Q2112/88Q2110 Ethernet PHYs
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/mii.h>
#include <linux/delay.h>
#include <linux/phy.h>

// 1000 BASE-T1 Operating Mode
#define MRVL_88Q211X_MODE_LEGACY    0x06B0
#define MRVL_88Q211X_MODE_DEFAULT   0x0000
#define MRVL_88Q211X_MODE_ADVERTISE 0x0002

// Current Speed
#define MRVL_88Q2112_1000BASE_T1    0x0001
#define MRVL_88Q2112_100BASE_T1     0x0000

enum phy_dr_mode {
	PHY_DR_MODE_UNKNOWN,
	PHY_DR_MODE_MASTER,
	PHY_DR_MODE_SLAVE,
	PHY_DR_MODE_AUTONEGO,
};

enum phy_device_speed {
	PHY_SPEED_UNKNOWN,
	PHY_SPEED_10M,
	PHY_SPEED_100M,
	PHY_SPEED_1000M,
};

struct phy_priv {
	enum phy_dr_mode dr_mode;
	enum phy_device_speed device_speed;
};

static const char *const phy_dr_modes[] = {
	[PHY_DR_MODE_UNKNOWN]		= "",
	[PHY_DR_MODE_MASTER]		= "master",
	[PHY_DR_MODE_SLAVE]		= "slave",
	[PHY_DR_MODE_AUTONEGO]		= "autonego",
};

static const char *const speed_names[] = {
	[PHY_SPEED_UNKNOWN]	= "UNKNOWN",
	[PHY_SPEED_10M]		= "10M-speed",
	[PHY_SPEED_100M]	= "100M-speed",
	[PHY_SPEED_1000M]	= "1000M-speed",
};

enum phy_dr_mode phy_get_dr_mode(struct device *dev)
{
	const char *dr_mode;
	int ret;

	ret = device_property_read_string(dev, "dr_mode", &dr_mode);
	if (ret < 0)
		return PHY_DR_MODE_UNKNOWN;

	ret = match_string(phy_dr_modes, ARRAY_SIZE(phy_dr_modes), dr_mode);
	return (ret < 0) ? PHY_DR_MODE_UNKNOWN : ret;
}

enum phy_device_speed phy_get_maximum_speed(struct device *dev)
{
	const char *maximum_speed;
	int ret;

	ret = device_property_read_string(dev, "maximum-speed", &maximum_speed);
	if (ret < 0)
		return PHY_SPEED_UNKNOWN;

	ret = match_string(speed_names, ARRAY_SIZE(speed_names), maximum_speed);

	return (ret < 0) ? PHY_SPEED_UNKNOWN : ret;
}

static int ma211x_read(struct phy_device *phydev, u32 devaddr, u32 regaddr)
{
	regaddr = MII_ADDR_C45 | devaddr << MII_DEVADDR_C45_SHIFT | regaddr;
	return phy_read(phydev, regaddr);
}

static int ma211x_write(struct phy_device *phydev, u32 devaddr, u32 regaddr, u16 val)
{
	regaddr = MII_ADDR_C45 | devaddr << MII_DEVADDR_C45_SHIFT | regaddr;
	return phy_write(phydev, regaddr, val);
}

/* return 1 enable autonego, return 0 disable autonego */
static int ma211x_get_autonego_enable(struct phy_device *phydev)
{
	return (0x0 != (ma211x_read(phydev, 7, 0x0200) & 0x1000));
}

static void ma211x_set_autonego_enalbe(struct phy_device *phydev, int is_enable)
{
	if (is_enable)
		ma211x_write(phydev, 7, 0x0200, 0x1200); //an_enalbe, an_restart
	else
		ma211x_write(phydev, 7, 0x0200, 0);
}

/* return 1 1000MB, return 0 100MB */
static int ma211x_get_speed(struct phy_device *phydev)
{
	if (ma211x_get_autonego_enable(phydev))
		return ((ma211x_read(phydev, 7, 0x801a) & 0x4000) >> 14);
	else
		return (ma211x_read(phydev, 1, 0x0834) & 0x0001);
}

static int ma211x_set_master(struct phy_device *phydev, int is_master)
{
	u16 data = ma211x_read(phydev, 1, 0x0834);

	if (is_master == PHY_DR_MODE_MASTER)
		data |= 0x4000;
	else
		data &= 0xBFFF;
	return ma211x_write(phydev, 1, 0x0834, data);
}

static int ma211x_check_link(struct phy_device *phydev)
{
	u32 data;

	if (ma211x_get_speed(phydev) == MRVL_88Q2112_1000BASE_T1) {
		data = ma211x_read(phydev, 3, 0x0901);
		data = ma211x_read(phydev, 3, 0x0901);
	} else
		data = ma211x_read(phydev, 3, 0x8109);
	if (0x4 == (data & 0x4))
		return 1;
	else
		return 0;
}

static int ma211x_ge_init(struct phy_device *phydev)
{
	u16 data;

	ma211x_write(phydev, 1, 0x0900, 0x4000);

	data = ma211x_read(phydev, 1, 0x0834);
	data = (data & 0xFFF0) | 0x0001;
	ma211x_write(phydev, 1, 0x0834, data);

	ma211x_write(phydev, 3, 0xFFE4, 0x07B5);
	ma211x_write(phydev, 3, 0xFFE4, 0x06B6);
	msleep(100);

	ma211x_write(phydev, 3, 0xFFDE, 0x402F);
	ma211x_write(phydev, 3, 0xFE2A, 0x3C3D);
	ma211x_write(phydev, 3, 0xFE34, 0x4040);
	//ma211x_write(phydev, 3, 0xFE4B, 0x9337);
	ma211x_write(phydev, 3, 0xFE2A, 0x3C1D);
	ma211x_write(phydev, 3, 0xFE34, 0x0040);
	ma211x_write(phydev, 7, 0x8032, 0x0064);
	ma211x_write(phydev, 7, 0x8031, 0x0A01);
	ma211x_write(phydev, 7, 0x8031, 0x0C01);
	ma211x_write(phydev, 3, 0xFE0F, 0x0000);
	ma211x_write(phydev, 3, 0x800C, 0x0000);
	ma211x_write(phydev, 3, 0x801D, 0x0800);
	ma211x_write(phydev, 3, 0xFC00, 0x01C0);
	ma211x_write(phydev, 3, 0xFC17, 0x0425);
	ma211x_write(phydev, 3, 0xFC94, 0x5470);
	ma211x_write(phydev, 3, 0xFC95, 0x0055);
	ma211x_write(phydev, 3, 0xFC19, 0x08D8);
	ma211x_write(phydev, 3, 0xFC1a, 0x0110);
	ma211x_write(phydev, 3, 0xFC1b, 0x0A10);
	ma211x_write(phydev, 3, 0xFC3A, 0x2725);
	ma211x_write(phydev, 3, 0xFC61, 0x2627);
	ma211x_write(phydev, 3, 0xFC3B, 0x1612);
	ma211x_write(phydev, 3, 0xFC62, 0x1C12);
	ma211x_write(phydev, 3, 0xFC9D, 0x6367);
	ma211x_write(phydev, 3, 0xFC9E, 0x8060);
	ma211x_write(phydev, 3, 0xFC00, 0x01C8);
	ma211x_write(phydev, 3, 0x8000, 0x0000);
	ma211x_write(phydev, 3, 0x8016, 0x0011);

	ma211x_write(phydev, 3, 0xFDA3, 0x1800);

	ma211x_write(phydev, 3, 0xFE02, 0x00C0);
	ma211x_write(phydev, 3, 0xFFDB, 0x0010);
	ma211x_write(phydev, 3, 0xFFF3, 0x0020);
	ma211x_write(phydev, 3, 0xFE40, 0x00A6);

	ma211x_write(phydev, 3, 0xFE60, 0x0000);
	ma211x_write(phydev, 3, 0xFE04, 0x0008);
	ma211x_write(phydev, 3, 0xFE2A, 0x3C3D);
	ma211x_write(phydev, 3, 0xFE4B, 0x9334);

	ma211x_write(phydev, 3, 0xFC10, 0xF600);
	ma211x_write(phydev, 3, 0xFC11, 0x073D);
	ma211x_write(phydev, 3, 0xFC12, 0x000D);
	//ma211x_write(phydev, 3, 0xFC13, 0x0010);

	return 0;
}

static int ma211x_ge_set_opmode(struct phy_device *phydev, u32 op_mode)
{
	if (op_mode == MRVL_88Q211X_MODE_LEGACY) {
		// Enable 1000 BASE-T1 legacy mode support
		ma211x_write(phydev, 3, 0xFDB8, 0x0001);
		ma211x_write(phydev, 3, 0xFD3D, 0x0C14);
	} else {
		// Set back to default compliant mode setting
		ma211x_write(phydev, 3, 0xFDB8, 0x0000);
		ma211x_write(phydev, 3, 0xFD3D, 0x0000);
	}
	ma211x_write(phydev, 1, 0x0902, op_mode | MRVL_88Q211X_MODE_ADVERTISE);
	return 0;
}

static int ma211x_ge_soft_reset(struct phy_device *phydev)
{
	u16 data;

	if (ma211x_get_autonego_enable(phydev))
		ma211x_write(phydev, 3, 0xFFF3, 0x0024);

	//enable low-power mode
	data = ma211x_read(phydev, 1, 0x0000);
	ma211x_write(phydev, 1, 0x0000, data | 0x0800);

	ma211x_write(phydev, 3, 0xFFF3, 0x0020);
	ma211x_write(phydev, 3, 0xFFE4, 0x000C);
	msleep(100);

	ma211x_write(phydev, 3, 0xffe4, 0x06B6);

	// disable low-power mode
	ma211x_write(phydev, 1, 0x0000, data & 0xF7FF);
	msleep(100);

	ma211x_write(phydev, 3, 0xFC47, 0x0030);
	ma211x_write(phydev, 3, 0xFC47, 0x0031);
	ma211x_write(phydev, 3, 0xFC47, 0x0030);
	ma211x_write(phydev, 3, 0xFC47, 0x0000);
	ma211x_write(phydev, 3, 0xFC47, 0x0001);
	ma211x_write(phydev, 3, 0xFC47, 0x0000);

	ma211x_write(phydev, 3, 0x0900, 0x8000);

	ma211x_write(phydev, 1, 0x0900, 0x0000);
	ma211x_write(phydev, 3, 0xFFE4, 0x000C);
	return 0;
}

static int ma211x_fe_init(struct phy_device *phydev)
{
	u16 data = 0;

	ma211x_write(phydev, 3, 0xFA07, 0x0202);

	data = ma211x_read(phydev, 1, 0x0834);
	data = data & 0xFFF0;
	ma211x_write(phydev, 1, 0x0834, data);
	msleep(50);

	ma211x_write(phydev, 3, 0x8000, 0x0000);
	ma211x_write(phydev, 3, 0x8100, 0x0200);
	ma211x_write(phydev, 3, 0xFA1E, 0x0002);
	ma211x_write(phydev, 3, 0xFE5C, 0x2402);
	ma211x_write(phydev, 3, 0xFA12, 0x001F);
	ma211x_write(phydev, 3, 0xFA0C, 0x9E05);
	ma211x_write(phydev, 3, 0xFBDD, 0x6862);
	ma211x_write(phydev, 3, 0xFBDE, 0x736E);
	ma211x_write(phydev, 3, 0xFBDF, 0x7F79);
	ma211x_write(phydev, 3, 0xFBE0, 0x8A85);
	ma211x_write(phydev, 3, 0xFBE1, 0x9790);
	ma211x_write(phydev, 3, 0xFBE3, 0xA39D);
	ma211x_write(phydev, 3, 0xFBE4, 0xB0AA);
	ma211x_write(phydev, 3, 0xFBE5, 0x00B8);
	ma211x_write(phydev, 3, 0xFBFD, 0x0D0A);
	ma211x_write(phydev, 3, 0xFBFE, 0x0906);
	ma211x_write(phydev, 3, 0x801D, 0x8000);
	ma211x_write(phydev, 3, 0x8016, 0x0011);

	// reset pcs
	ma211x_write(phydev, 3, 0x0900, 0x8000);
	ma211x_write(phydev, 3, 0xFA07, 0x0200);
	msleep(100);

	ma211x_write(phydev, 31, 0x8001, 0xc000);
	return 0;
}

static int ma211x_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct phy_priv *priv;
	u32 id = 0;

	if (!phydev->is_c45)
		return -ENOTSUPP;

	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dr_mode = phy_get_dr_mode(dev);
	priv->device_speed = phy_get_maximum_speed(dev);
	phydev->priv = priv;

	printk("%s dr_mode %d maximum_speed %d\n",
	       __func__, priv->dr_mode, priv->device_speed);

	if (priv->dr_mode == PHY_DR_MODE_UNKNOWN)
		priv->dr_mode =  PHY_DR_MODE_MASTER;
	if (priv->device_speed == PHY_SPEED_UNKNOWN)
		priv->device_speed = PHY_SPEED_1000M;
	id = ma211x_read(phydev, 1, 2) << 16 | ma211x_read(phydev, 1, 3);
	printk("%s get phy id[%x]\n", __func__, id);
	return 0;
}

static int ma211x_soft_reset(struct phy_device *phydev)
{
	return 0;
}

static int ma211x_config_init(struct phy_device *phydev)
{
	struct phy_priv *priv = phydev->priv;

	if (priv->device_speed ==  PHY_SPEED_100M) {
		ma211x_set_master(phydev, priv->dr_mode);
		ma211x_set_autonego_enalbe(phydev, 0);
		ma211x_fe_init(phydev);
	} else if (priv->device_speed ==  PHY_SPEED_1000M) {
		ma211x_set_master(phydev, priv->dr_mode);
		ma211x_set_autonego_enalbe(phydev, 0);
		ma211x_ge_init(phydev);
		ma211x_ge_set_opmode(phydev, MRVL_88Q211X_MODE_DEFAULT);
		ma211x_ge_soft_reset(phydev);
	} else {
		phydev_err(phydev, "not supported\n");
		return -ENOTSUPP;
	}

	return 0;
}

static int ma211x_config_aneg(struct phy_device *phydev)
{
	phydev->supported = SUPPORTED_100baseT_Full | SUPPORTED_1000baseT_Full;
	phydev->advertising = SUPPORTED_100baseT_Full | SUPPORTED_1000baseT_Full;
	return 0;
}

static int ma211x_aneg_done(struct phy_device *phydev)
{
	return 1;
}

static int ma211x_read_status(struct phy_device *phydev)
{
	if (ma211x_get_speed(phydev))
		phydev->speed = SPEED_1000;
	else
		phydev->speed = SPEED_100;

	if (ma211x_check_link(phydev))
		phydev->link = 1;
	else
		phydev->link = 0;
	phydev->duplex = DUPLEX_FULL;
	phydev->pause = 0;
	phydev->asym_pause = 0;
	return 0;
}

static int ma1512_probe(struct phy_device *phydev)
{
	return 0;
}

static int ma1512_config_init(struct phy_device *phydev)
{
	phy_write(phydev, 0, 0x9140);
	return 0;
}

static int ma1512_read_status(struct phy_device *phydev)
{
	int val;

	val = phy_read(phydev, 17);
	if (((val >> 14) & 0x3) == 0)
		phydev->speed = SPEED_10;
	else if (((val >> 14) & 0x3) == 1)
		phydev->speed = SPEED_100;
	else if (((val >> 14) & 0x3) == 2)
		phydev->speed = SPEED_1000;

	if ((val >> 10) & 0x1)
		phydev->link = 1;
	else
		phydev->link = 0;

	if ((val >> 13) & 0x1)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;
	phydev->pause = 0;
	phydev->asym_pause = 0;
	return 0;
}

static int rtl9010_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct phy_priv *priv;
	int val2, val3;

	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dr_mode = phy_get_dr_mode(dev);
	priv->device_speed = phy_get_maximum_speed(dev);
	phydev->priv = priv;

	printk("%s dr_mode %d maximum_speed %d\n",
	       __func__, priv->dr_mode, priv->device_speed);

	if (priv->dr_mode == PHY_DR_MODE_UNKNOWN)
		priv->dr_mode =  PHY_DR_MODE_MASTER;
	if (priv->device_speed == PHY_SPEED_UNKNOWN)
		priv->device_speed = PHY_SPEED_1000M;

	val2 = phy_read(phydev, 2);
	val3 = phy_read(phydev, 3);
	printk("%s phyid %x%x\n", __func__, val2, val3);
	return 0;
}

static int rtl9010_config_init(struct phy_device *phydev)
{
	struct phy_priv *priv = phydev->priv;
	u32 val = 0;
	u32 timer = 2000;

	/* PHY Parameter Start */
	phy_write(phydev, 31, 0x0BC4);
	phy_write(phydev, 21, 0x16FE);
	phy_write(phydev, 27, 0xB820);
	phy_write(phydev, 28, 0x0010);
	phy_write(phydev, 27, 0xB830);
	phy_write(phydev, 28, 0x8000);
	phy_write(phydev, 27, 0xB800);

	while (!(phy_read(phydev, 28) & 0x0040)) {
		phy_write(phydev, 27, 0xB800);
		timer--;
		if (timer == 0)
			return -1;
		udelay(1);
	}

	phy_write(phydev, 27, 0x8020);
	phy_write(phydev, 28, 0x9100);
	phy_write(phydev, 27, 0xB82E);
	phy_write(phydev, 28, 0x0001);
	phy_write(phydev, 27, 0xB820);
	phy_write(phydev, 28, 0x0290);
	phy_write(phydev, 27, 0xA012);
	phy_write(phydev, 28, 0x0000);
	phy_write(phydev, 27, 0xA014);
	phy_write(phydev, 28, 0xD700);
	phy_write(phydev, 28, 0x880F);
	phy_write(phydev, 28, 0x262D);
	phy_write(phydev, 27, 0xA01A);
	phy_write(phydev, 28, 0x0000);
	phy_write(phydev, 27, 0xA000);
	phy_write(phydev, 28, 0x162C);
	phy_write(phydev, 27, 0xB820);
	phy_write(phydev, 28, 0x0210);
	phy_write(phydev, 27, 0xB82E);
	phy_write(phydev, 28, 0x0000);
	phy_write(phydev, 27, 0x8020);
	phy_write(phydev, 28, 0x0000);
	phy_write(phydev, 27, 0xB820);
	phy_write(phydev, 28, 0x0000);

	timer = 2000;
	phy_write(phydev, 27, 0xB800);
	while (phy_read(phydev, 28) & 0x0040) {
		phy_write(phydev, 27, 0xB800);
		timer--;
		if (timer == 0)
			return -1;
		udelay(1);
	}

	/* Set rx delay to 2ns. */
	phy_write(phydev, 27, 0xD04A);
	phy_write(phydev, 28, 7);

	/* Set tx delay to 2ns. */
	phy_write(phydev, 27, 0xD084);
	phy_write(phydev, 28, 0x4007);
	/* PHY soft-reset */
	phy_write(phydev, 0, 0x8000);
	/* Check soft-reset complete */
	while (val != 0x0140) {
		val = phy_read(phydev, 0);
	}

	if (priv->device_speed ==  PHY_SPEED_100M)
		phy_write(phydev, 0, 0x2100);

	val = phy_read(phydev, 9);
	if (priv->dr_mode == PHY_DR_MODE_MASTER)
		val |= (1 << 11);
	else
		val &= ~(1 << 11);
	phy_write(phydev, 9, val);
	return 0;
}

static int rtl9010_read_status(struct phy_device *phydev)
{
	int val;

	val = phy_read(phydev, 1);
	val = phy_read(phydev, 1);
	if (val & 0x4)
		phydev->link = 1;
	else
		phydev->link = 0;

	val = phy_read(phydev, 0x1a);

	if (((val >> 4) & 0x3) == 0x1)
		phydev->speed = SPEED_100;
	else if (((val >> 4) & 0x3) == 0x2)
		phydev->speed = SPEED_1000;

	if (val & 0x8)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;
	phydev->pause = 0;
	phydev->asym_pause = 0;

	return 0;
}

static struct phy_driver marvellauto_driver[] = { {
	.phy_id		= 0x002b0983,
	.name		= "ma211x",
	.phy_id_mask	= 0xfffffff0,
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_1000baseT_Full),
	.flags		= PHY_POLL,
	.soft_reset	= ma211x_soft_reset,
	.probe		= ma211x_probe,
	.config_init	= ma211x_config_init,
	.config_aneg	= ma211x_config_aneg,
	.aneg_done	= ma211x_aneg_done,
	.read_status	= ma211x_read_status,
},{
	.phy_id		= 0x01410dd4,
	.name		= "ma1512",
	.phy_id_mask	= 0xfffffff0,
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_1000baseT_Full),
	.flags		= PHY_POLL,
	.soft_reset	= ma211x_soft_reset,
	.probe		= ma1512_probe,
	.config_init	= ma1512_config_init,
	.config_aneg	= ma211x_config_aneg,
	.aneg_done	= ma211x_aneg_done,
	.read_status	= ma1512_read_status,
},{
	.phy_id		= 0x001ccb30,
	.name		= "RTL9010 Gigabit T1 Eth",
	.phy_id_mask	= 0x001ffff0,
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_1000baseT_Full),
	.flags		= PHY_POLL,
	.soft_reset	= ma211x_soft_reset,
	.probe		= rtl9010_probe,
	.config_init	= rtl9010_config_init,
	.config_aneg	= ma211x_config_aneg,
	.aneg_done	= ma211x_aneg_done,
	.read_status	= rtl9010_read_status,
} };

module_phy_driver(marvellauto_driver);

static struct mdio_device_id __maybe_unused marvellauto_tbl[] = {
	{ 0x002b0938, 0xfffffff0 },
	{ 0x01410dd4, 0xfffffff0 },
	{ 0x001ccb30, 0x01fffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, marvellauto_tbl);

MODULE_DESCRIPTION("Marvell Automotive 88Q2112/88Q2110 PHY driver");
MODULE_AUTHOR("Dejin Zheng");
MODULE_LICENSE("GPL");
