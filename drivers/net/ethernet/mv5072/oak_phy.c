#include "oak_unimac.h"

#define PHY_C45			true
#define PHY_C22			false
#define SW_INTERNAL_PHY		0
#define SW_EXTERNAL_PHY		1

#define SW_GLOBAL2_PORT		0x1c
#define SMI_PHY_CMD_REG		0x18
#define SMI_PHY_DATA_REG	0x19
#define SMI_BUSY_OFFSET		15
#define SMI_FUNC_OFFSET		13
#define SMI_OP_OFFSET		10
#define SMI_DEV_ADDR_OFFSET	5

/*switch port write*/
void swp_write(void *addr, int port, int reg, int data)
{
	writel((data), addr + ((port * 32 + reg) * 4));
}

/*switch port read*/
int swp_read(void *addr, int port, int reg)
{
	return readl(addr + ((port * 32 + reg) * 4));
}

bool swphy_is_busy(void *base, int timeout)
{
	int val;

	do {
		udelay(1);
		val = swp_read(base, SW_GLOBAL2_PORT, SMI_PHY_CMD_REG);
	} while (--timeout && (val & (1 << SMI_BUSY_OFFSET)));

	if (!timeout) {
		pr_err("%s found phy is busy\n", __func__);
		return true;
	}
	return false;
}

/*switch phy write*/
int swphy_write(void *base, int is_c45, int is_internal,
		int phyid, int devnum, int reg, int data)
{
	u16 cmd = 0;

	if (swphy_is_busy(base, 100))
		return -EBUSY;

	cmd = (1 << SMI_BUSY_OFFSET) | (is_internal << SMI_FUNC_OFFSET)
		| (phyid << SMI_DEV_ADDR_OFFSET);

	if (is_c45) {
		cmd |= devnum;
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_DATA_REG, reg);
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_CMD_REG, cmd);
	} else {
		cmd |= (1 << SMI_OP_OFFSET) | (1 << 12) | reg;
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_DATA_REG, data);
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_CMD_REG, cmd);
	}

	if (is_c45) {
		if (swphy_is_busy(base, 100))
			return -EBUSY;
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_DATA_REG, data);
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_CMD_REG,
				cmd | (1 << SMI_OP_OFFSET));
	}

	if (swphy_is_busy(base, 100))
		return -EBUSY;
	return 0;
}

/*switch phy read*/
int swphy_read(void *base, int is_c45, int is_internal,
	       int phyid, int devnum, int reg)
{
	u16 cmd = 0;

	if (swphy_is_busy(base, 100))
		return -EBUSY;

	cmd = (1 << SMI_BUSY_OFFSET) | (is_internal << SMI_FUNC_OFFSET)
		| (phyid << SMI_DEV_ADDR_OFFSET);

	if (is_c45) {
		cmd |= devnum;
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_DATA_REG, reg);
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_CMD_REG, cmd);
	} else {
		cmd |= (2 << SMI_OP_OFFSET) | (1 << 12) | reg;
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_CMD_REG, cmd);
	}

	if (is_c45) {
		if (swphy_is_busy(base, 100))
			return -EBUSY;
		swp_write(base, SW_GLOBAL2_PORT, SMI_PHY_CMD_REG,
				cmd | (3 << SMI_OP_OFFSET));
	}

	if (swphy_is_busy(base, 100))
		return -EBUSY;
	return swp_read(base, SW_GLOBAL2_PORT, SMI_PHY_DATA_REG);
}

void sw_88q222x_int(void *swp, int phyid)
{
	int val;

	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x7, 0x0200, 0x0000);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x1, 0x0000, 0x0840);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x3, 0xFE1B, 0x0048);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x3, 0xFFE4, 0x06B6);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x1, 0x0000, 0x0000);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x3, 0x0000, 0x0000);
	msleep(100);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x7, 0x8032, 0x2020);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x7, 0x8031, 0x0A28);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x7, 0x8031, 0x0C28);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AB, 0x0001);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AD, 0x0000);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AC, 0x0042);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AE, 0x144A);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AC, 0x0050);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AE, 0xF800);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AC, 0x0051);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AE, 0x0970);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AC, 0x0043);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x4, 0x80AE, 0x56AD);

	val = swphy_read(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x1, 0x0834);
	val |= 0x1;
	val &= ~(1 << 14);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x1, 0x0834, val);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x3, 0x900, 0x8000);
	swphy_write(swp, PHY_C45, SW_EXTERNAL_PHY, phyid, 0x3, 0xFFE4, 0x000C);
}

void g9h_int(void *swp)
{
	int i, tmp;

	/* P1 ~ P6 SGMII port init*/
	for (i = 1; i < 7; i++) {
		sw_88q222x_int(swp, i);

		tmp = i + 16;
		swphy_write(swp, PHY_C22, SW_INTERNAL_PHY, tmp, 0, 22, 1);
		swphy_write(swp, PHY_C22, SW_INTERNAL_PHY, tmp, 0, 0, 0x9140);
		swphy_write(swp, PHY_C22, SW_INTERNAL_PHY, tmp, 0, 22, 0);
	}

	for (i = 1; i < 11; i++)
		swp_write(swp, i, 0x4, 0x7f);

	swp_write(swp, 7, 0x1, 0x203a);
	swp_write(swp, 8, 0x1, 0x203a);
}

int sw_phy_int(oak_t *np)
{
	if (np->pdev->device == 0x0f13)
		g9h_int(np->um_base + 0x10000);

	return 0;
}
