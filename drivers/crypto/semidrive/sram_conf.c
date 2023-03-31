/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include "sram_conf.h"
#include "ce_reg.h"
#include "ce.h"

void sram_config(void)
{
	uint32_t value;

	value = reg_value(CE0_SRAM_SIZE, 0, CE0_RAM_SIZE_SHIFT, CE0_RAM_SIZE_MASK);
	value = reg_value(CE1_SRAM_SIZE, value, CE1_RAM_SIZE_SHIFT, CE1_RAM_SIZE_MASK);
	value = reg_value(CE2_SRAM_SIZE, value, CE2_RAM_SIZE_SHIFT, CE2_RAM_SIZE_MASK);
	value = reg_value(CE3_SRAM_SIZE, value, CE3_RAM_SIZE_SHIFT, CE3_RAM_SIZE_MASK);
	writel(value, (void *)_ioaddr(REG_CE_SESRAM_SIZE));

	value = reg_value(CE4_SRAM_SIZE, 0, CE4_RAM_SIZE_SHIFT, CE4_RAM_SIZE_MASK);
	value = reg_value(CE5_SRAM_SIZE, value, CE5_RAM_SIZE_SHIFT, CE5_RAM_SIZE_MASK);
	value = reg_value(CE6_SRAM_SIZE, value, CE6_RAM_SIZE_SHIFT, CE6_RAM_SIZE_MASK);
	value = reg_value(CE7_SRAM_SIZE, value, CE7_RAM_SIZE_SHIFT, CE7_RAM_SIZE_MASK);
	writel(value, (void *)_ioaddr(REG_CE_SESRAM_SIZE_CONT));

	value = reg_value(CE0_SEC_SRAM_SIZE, 0, CE0_RAM_SASIZE_SHIFT, CE0_RAM_SASIZE_MASK);
	value = reg_value(CE1_SEC_SRAM_SIZE, value, CE1_RAM_SASIZE_SHIFT, CE1_RAM_SASIZE_MASK);
	value = reg_value(CE2_SEC_SRAM_SIZE, value, CE2_RAM_SASIZE_SHIFT, CE2_RAM_SASIZE_MASK);
	value = reg_value(CE3_SEC_SRAM_SIZE, value, CE3_RAM_SASIZE_SHIFT, CE3_RAM_SASIZE_MASK);
	writel(value, (void *)_ioaddr(REG_CE_SESRAM_SASIZE));

	value = reg_value(CE4_SEC_SRAM_SIZE, 0, CE4_RAM_SASIZE_SHIFT, CE4_RAM_SASIZE_MASK);
	value = reg_value(CE5_SEC_SRAM_SIZE, value, CE5_RAM_SASIZE_SHIFT, CE5_RAM_SASIZE_MASK);
	value = reg_value(CE6_SEC_SRAM_SIZE, value, CE6_RAM_SASIZE_SHIFT, CE6_RAM_SASIZE_MASK);
	value = reg_value(CE7_SEC_SRAM_SIZE, value, CE7_RAM_SASIZE_SHIFT, CE7_RAM_SASIZE_MASK);
	writel(value, (void *)_ioaddr(REG_CE_SESRAM_SASIZE_CONT));
}

addr_t sram_pub_addr(uint32_t ce_id)
{
	addr_t addr_base = 0;
	uint32_t i;
	uint32_t sram_size1, sram_size2 = 0;

	sram_size1 = readl((void *)_ioaddr(REG_CE_SESRAM_SIZE));

	if (ce_id > 3) {
		sram_size2 = readl((void *)_ioaddr(REG_CE_SESRAM_SIZE_CONT));
	}

	for (i = 0; i < ce_id; i++) {
		addr_base += i > 3 ? (sram_size2 >> (i % 4) * 8) & 0x7F : (sram_size1 >> i * 8) & 0x7F;
	}

	return SRAM_BASE_ADDR + addr_base * 1024;
}

addr_t sram_sec_addr(uint32_t ce_id)
{
	uint32_t sram_sec_size;

	sram_sec_size = ce_id > 3 ? (readl((void *)_ioaddr(REG_CE_SESRAM_SASIZE_CONT)) >> (ce_id % 4) * 8) & 0x7F
					: (readl((void *)_ioaddr(REG_CE_SESRAM_SASIZE)) >> ce_id * 8) & 0x7F;

	if (7 == ce_id) {
		return SRAM_BASE_ADDR + (SRAM_SIZE - sram_sec_size) * 1024;
	}

	return sram_pub_addr(ce_id + 1) - sram_sec_size * 1024;
}

addr_t sram_addr(ce_addr_type_t addr_type, uint32_t ce_id)
{
	if (SRAM_PUB == addr_type) {
		return sram_pub_addr(ce_id);
	}
	else if (SRAM_SEC == addr_type) {
		return sram_sec_addr(ce_id);
	}
	else {
		return 0;
	}
}

addr_t get_sram_base(uint32_t vce_id)
{
	addr_t addr_base;

	switch (vce_id) {
		case 0:
			addr_base = SRAM_BASE_ADDR;
			break;

		case 1:
			addr_base = SRAM_BASE_ADDR + CE0_SRAM_SIZE * 1024;
			break;

		case 2:
			addr_base = SRAM_BASE_ADDR + (CE0_SRAM_SIZE + CE1_SRAM_SIZE) * 1024;
			break;

		case 3:
			addr_base = SRAM_BASE_ADDR + (CE0_SRAM_SIZE + CE1_SRAM_SIZE
										  + CE2_SRAM_SIZE) * 1024;
			break;

		case 4:
			addr_base = SRAM_BASE_ADDR + (CE0_SRAM_SIZE + CE1_SRAM_SIZE
										  + CE2_SRAM_SIZE + CE3_SRAM_SIZE) * 1024;
			break;

		case 5:
			addr_base = SRAM_BASE_ADDR + (CE0_SRAM_SIZE + CE1_SRAM_SIZE
										  + CE2_SRAM_SIZE + CE3_SRAM_SIZE + CE4_SRAM_SIZE) * 1024;
			break;

		case 6:
			addr_base = SRAM_BASE_ADDR + (CE0_SRAM_SIZE + CE1_SRAM_SIZE
										  + CE2_SRAM_SIZE + CE3_SRAM_SIZE + CE4_SRAM_SIZE
										  + CE5_SRAM_SIZE) * 1024;
			break;

		case 7:
			addr_base = SRAM_BASE_ADDR + (CE0_SRAM_SIZE + CE1_SRAM_SIZE
										  + CE2_SRAM_SIZE + CE3_SRAM_SIZE + CE4_SRAM_SIZE
										  + CE5_SRAM_SIZE + CE6_SRAM_SIZE) * 1024;
			break;

		default:
			return 0;
	}

	return addr_base;
}
