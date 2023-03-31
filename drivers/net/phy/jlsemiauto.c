/**
 *  Driver for JLSemi Automotive JL31XX Ethernet PHYs
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/mii.h>
#include <linux/delay.h>
#include <linux/phy.h>

#define JL31XX_PHY_ID		0x937c4030

#define JL31XX_SPEED_NOTSUPP	0x0000
#define JL31XX_100BASE_T1	0x0001
#define JL31XX_1000BASE_T1	0x0002
#define JL31XX_SLAVE_MODE	0x0000
#define JL31XX_MASTER_MODE	0x0001
#define JL31XX_PMA_CTL_ADDR	0x0001
#define JL31XX_PMA_CTL		0x0834
#define JL31XX_MASTER		BIT(14)
#define JL31XX_PMA_BASE		0x0001
#define JL31XX_PMA_RESET	BIT(15)
#define JL31XX_PHYSID1		0x0002
#define JL31XX_PHYSID2		0x0003

#define JL31XX_PCS_CTL_ADDR	0x0003
#define JL31XX_PCS_CTL		0x0000
#define JL31XX_PCS_SPEED_LSB	BIT(13)
#define JL31XX_PCS_SPEED_MSB	BIT(6)

#define JL31XX_IEEE_CTL_ADDR	0x001f
#define JL31XX_IEEE_CTL		0x0471
#define JL31XX_GLOBAL_LINK	BIT(0)
#define JL31XX_RGMII_CFG	0x3400
#define JL31XX_RGMII_TX_DLY	BIT(4)

enum jl31xx_ms_mode {
	MS_MODE_UNKNOWN,
	MS_MODE_MASTER,
	MS_MODE_SLAVE,
};

enum jl31xx_phy_speed {
	PHY_SPEED_UNKNOWN,
	PHY_SPEED_1000M,
	PHY_SPEED_100M,
};

struct jl31xx_priv {
	enum jl31xx_ms_mode ms_mode;
	enum jl31xx_phy_speed phy_speed;
};

static const char *const jl31xx_modes[] = {
	[MS_MODE_UNKNOWN]	= "uknown",
	[MS_MODE_MASTER]	= "master",
	[MS_MODE_SLAVE]		= "slave",
};

static const char *const speed_names[] = {
	[PHY_SPEED_UNKNOWN]	= "uknown",
	[PHY_SPEED_1000M]	= "1000M",
	[PHY_SPEED_100M]		= "100M",
};

enum jl31xx_ms_mode get_ms_mode(struct device *dev)
{
	const char *ms_mode;
	int ret;

	ret = device_property_read_string(dev, "ms_mode", &ms_mode);
	if (ret < 0)
		return MS_MODE_UNKNOWN;

	ret = match_string(jl31xx_modes, ARRAY_SIZE(jl31xx_modes), ms_mode);
	if (ret < 0)
		return MS_MODE_UNKNOWN;

	return ret;
}

enum jl31xx_phy_speed get_phy_speed(struct device *dev)
{
	const char *phy_speed;
	int ret;

	ret = device_property_read_string(dev, "speed", &phy_speed);
	if (ret < 0)
		return PHY_SPEED_UNKNOWN;

	ret = match_string(speed_names, ARRAY_SIZE(speed_names), phy_speed);
	if (ret < 0)
		return PHY_SPEED_UNKNOWN;

	return ret;
}

static int jl31xx_mmd_modify(struct phy_device *phydev,
			     int devad, u16 reg, u16 mask, u16 bits)
{
	int old, val, ret;

	old = phy_read_mmd(phydev, devad, reg);
	if (old < 0)
		return old;

	val = (old & ~mask) | bits;
	if (val == old)
		return 0;

	ret = phy_write_mmd(phydev, devad, reg, val);

	return ret < 0 ? ret : 1;
}

static int jl31xx_fetch_mmd_bit(struct phy_device *phydev,
				int devad, u16 reg, u16 bit)
{
	int val;

	val = phy_read_mmd(phydev, devad, reg);
	if (val < 0)
		return val;

	return ((val & bit) == bit) ? 1 : 0;
}

static int jl31xx_set_mmd_bits(struct phy_device *phydev,
			       int devad, u16 reg, u16 bits)
{
	return jl31xx_mmd_modify(phydev, devad, reg, 0, bits);
}

static int jl31xx_clear_mmd_bits(struct phy_device *phydev,
				 int devad, u16 reg, u16 bits)
{
	return jl31xx_mmd_modify(phydev, devad, reg, bits, 0);
}

static int jl31xx_check_link(struct phy_device *phydev)
{
	return jl31xx_fetch_mmd_bit(phydev, JL31XX_IEEE_CTL_ADDR,
				    JL31XX_IEEE_CTL, JL31XX_GLOBAL_LINK);
}

static int jl31xx_set_force_ms_mode(struct phy_device *phydev,
				    enum jl31xx_ms_mode mode)
{
	if (mode == MS_MODE_MASTER)
		jl31xx_set_mmd_bits(phydev, JL31XX_PMA_CTL_ADDR,
				    JL31XX_PMA_CTL, JL31XX_MASTER);
	else
		jl31xx_clear_mmd_bits(phydev, JL31XX_PMA_CTL_ADDR,
				      JL31XX_PMA_CTL, JL31XX_MASTER);

	return 0;
}

static int jl31xx_get_force_ms_mode(struct phy_device *phydev)
{
	int val;

	val = jl31xx_fetch_mmd_bit(phydev, JL31XX_PMA_CTL_ADDR,
				   JL31XX_PMA_CTL, JL31XX_MASTER);
	if (val)
		return JL31XX_MASTER_MODE;
	else
		return JL31XX_SLAVE_MODE;
}

static int jl31xx_set_force_phy_speed(struct phy_device *phydev,
				     enum jl31xx_phy_speed speed)
{
	if (speed == PHY_SPEED_1000M)
		jl31xx_mmd_modify(phydev, JL31XX_PCS_CTL_ADDR, JL31XX_PCS_CTL,
				  JL31XX_PCS_SPEED_LSB, JL31XX_PCS_SPEED_MSB);
	else
		jl31xx_mmd_modify(phydev, JL31XX_PCS_CTL_ADDR, JL31XX_PCS_CTL,
				  JL31XX_PCS_SPEED_MSB, JL31XX_PCS_SPEED_LSB);

	return 0;
}

static int jl31xx_get_force_phy_speed(struct phy_device *phydev)
{
	int msb, lsb;

	msb = jl31xx_fetch_mmd_bit(phydev, JL31XX_PCS_CTL_ADDR,
				   JL31XX_PCS_CTL, JL31XX_PCS_SPEED_MSB);

	lsb = jl31xx_fetch_mmd_bit(phydev, JL31XX_PCS_CTL_ADDR,
				   JL31XX_PCS_CTL, JL31XX_PCS_SPEED_LSB);

	if (~lsb & msb)
		return JL31XX_1000BASE_T1;
	else if (lsb & ~msb)
		return JL31XX_100BASE_T1;
	else
		return JL31XX_SPEED_NOTSUPP;
}
static int jl31xx_turn_on_tx_dly(struct phy_device *phydev)
{
	return jl31xx_set_mmd_bits(phydev, JL31XX_IEEE_CTL_ADDR,
				   JL31XX_RGMII_CFG, JL31XX_RGMII_TX_DLY);
}

static int jl31xx_config_phy_status(struct phy_device *phydev,
				    struct jl31xx_priv *priv)
{
	int mode, speed;

	mode = jl31xx_get_force_ms_mode(phydev);
	speed = jl31xx_get_force_phy_speed(phydev);

	if (priv->ms_mode == MS_MODE_UNKNOWN) {
		if (mode == JL31XX_MASTER_MODE)
			priv->ms_mode = MS_MODE_MASTER;
		else
			priv->ms_mode = MS_MODE_SLAVE;
	}
	if (priv->phy_speed == PHY_SPEED_UNKNOWN) {
		if (speed == JL31XX_1000BASE_T1)
			priv->phy_speed = PHY_SPEED_1000M;
		else if (speed == JL31XX_100BASE_T1)
			priv->phy_speed = PHY_SPEED_100M;
		else
			return -EOPNOTSUPP;
	}

	return 0;
}

static int jl31xx_soft_reset(struct phy_device *phydev)
{
	/* No soft reset about jl31xx in kernel phy interface */
	return 0;
}

static int jl31xx_config_init(struct phy_device *phydev)
{
	struct jl31xx_priv *priv = phydev->priv;
	int err;

	phydev->supported = SUPPORTED_100baseT_Full |
			    SUPPORTED_1000baseT_Full;
	phydev->advertising = SUPPORTED_100baseT_Full |
			      SUPPORTED_1000baseT_Full;

	err = jl31xx_set_force_phy_speed(phydev, priv->phy_speed);
	if (err < 0)
		return err;

	err = jl31xx_set_force_ms_mode(phydev, priv->ms_mode);
	if (err < 0)
		return err;

	err = jl31xx_set_mmd_bits(phydev, JL31XX_PMA_CTL_ADDR,
				  JL31XX_PMA_BASE, JL31XX_PMA_RESET);
	if (err < 0)
		return err;

	/* Wait soft reset to complete */
	msleep(50);

	return 0;
}

static int jl31xx_config_aneg(struct phy_device *phydev)
{
	/* Automatic negotiation is not supported at present */
	return 0;
}

static int jl31xx_aneg_done(struct phy_device *phydev)
{
	/* Automatic negotiation is not supported at present */
	return 1;
}

static int jl31xx_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct jl31xx_priv *priv;
	u32 id;

	if (!phydev->is_c45)
		return -ENOTSUPP;

	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->ms_mode = get_ms_mode(dev);
	priv->phy_speed = get_phy_speed(dev);
	phydev->priv = priv;

	dev_info(&phydev->mdio.dev, "ms_mode: %d phy_speed: %d\n",
		priv->ms_mode, priv->phy_speed);

	/* If the device tree is not initialized, we will initialize phy */
	jl31xx_config_phy_status(phydev, priv);

	id = phy_read_mmd(phydev, JL31XX_PMA_CTL_ADDR, JL31XX_PHYSID1) << 16 |
	     phy_read_mmd(phydev, JL31XX_PMA_CTL_ADDR, JL31XX_PHYSID2);

	dev_info(&phydev->mdio.dev, "phy_id: 0x%x\n", id);

	return 0;
}

static int jl31xx_read_status(struct phy_device *phydev)
{
	int speed;

	speed = jl31xx_get_force_phy_speed(phydev);

	if (speed == JL31XX_1000BASE_T1)
		phydev->speed = SPEED_1000;
	else if (speed == JL31XX_100BASE_T1)
		phydev->speed = SPEED_100;
	else
		phydev->speed = SPEED_UNKNOWN;

	if (jl31xx_check_link(phydev))
		phydev->link = 1;
	else
		phydev->link = 0;

	phydev->duplex = DUPLEX_FULL;
	phydev->pause = 0;
	phydev->asym_pause = 0;

	if (phydev->link)
		jl31xx_turn_on_tx_dly(phydev);

	return 0;
}

static struct phy_driver jlsemiauto_driver[] = {
{
	.phy_id		= JL31XX_PHY_ID,
	.name		= "jl31xx",
	.phy_id_mask	= 0xfffffff0,
	.features	= (SUPPORTED_100baseT_Full |
			  SUPPORTED_1000baseT_Full),
	.flags		= PHY_POLL,
	.soft_reset	= jl31xx_soft_reset,
	.probe		= jl31xx_probe,
	.config_init	= jl31xx_config_init,
	.config_aneg	= jl31xx_config_aneg,
	.aneg_done	= jl31xx_aneg_done,
	.read_status	= jl31xx_read_status,
} };

module_phy_driver(jlsemiauto_driver);

static struct mdio_device_id __maybe_unused jlsemiauto_tbl[] = {
	{ JL31XX_PHY_ID, 0xfffffff0 },
	{ },
};

MODULE_DEVICE_TABLE(mdio, jlsemiauto_tbl);

MODULE_DESCRIPTION("JLSemi Automotive JL31XX PHY driver");
MODULE_AUTHOR("gqkuang@jlsemi.com");
MODULE_LICENSE("GPL");
