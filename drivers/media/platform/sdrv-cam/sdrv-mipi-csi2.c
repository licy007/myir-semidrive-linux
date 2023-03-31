/*
 * sdrv-mipi-csi2.c
 *
 * Semidrive platform kstream subdev operation
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */


#include "sdrv-mipi-csi2.h"
#include <linux/delay.h>
#include <linux/io.h>

/**
 * @short Video formats supported by the MIPI CSI-2
 */
static const struct mipi_fmt dw_mipi_csi_formats[] = {
	{
		.name = "RAW BGGR 8",
		.code = MEDIA_BUS_FMT_SBGGR8_1X8,
		.depth = 8,
	},
	{
		.name = "RAW10",
		.code = MEDIA_BUS_FMT_SBGGR10_2X8_PADHI_BE,
		.depth = 10,
	},
	{
		.name = "RGB565",
		.code = MEDIA_BUS_FMT_RGB565_2X8_BE,
		.depth = 16,
	},
	{
		.name = "BGR565",
		.code = MEDIA_BUS_FMT_RGB565_2X8_LE,
		.depth = 16,
	},
	{
		.name = "RGB888",
		.code = MEDIA_BUS_FMT_RGB888_2X12_LE,
		.depth = 24,
	},
	{
		.name = "BGR888",
		.code = MEDIA_BUS_FMT_RGB888_2X12_BE,
		.depth = 24,
	},
};

static const struct phy_freqrange g_phy_freqs[63] = {
	{ 0x00, 80, 88},	//80
	{ 0x10, 88, 95},	//90
	{ 0x20, 95, 105},   //100
	{ 0x30, 105, 115},  //110
	{ 0x01, 115, 125},  //120
	{ 0x11, 125, 135},  //130
	{ 0x21, 135, 145},  //140
	{ 0x31, 145, 155},  //150
	{ 0x02, 155, 165},  //160
	{ 0x12, 165, 175},  //170
	{ 0x22, 175, 185},  //180
	{ 0x32, 185, 198},  //190
	{ 0x03, 198, 212},  //205
	{ 0x13, 212, 230},  //220
	{ 0x23, 230, 240},  //235
	{ 0x33, 240, 260},  //250
	{ 0x04, 260, 290},  //275
	{ 0x14, 290, 310},  //300
	{ 0x25, 310, 340},  //325
	{ 0x35, 340, 375},  //350
	{ 0x05, 375, 425},  //400
	{ 0x16, 425, 275},  //450
	{ 0x26, 475, 525},  //500
	{ 0x37, 525, 575},  //550
	{ 0x07, 575, 625},  //600
	{ 0x18, 625, 675},  //650
	{ 0x28, 675, 725},  //700
	{ 0x39, 725, 775},  //750
	{ 0x09, 775, 825},  //800
	{ 0x19, 825, 875},  //850
	{ 0x29, 875, 925},  //900
	{ 0x3a, 925, 975},  //950
	{ 0x0a, 975, 1025}, //1000
	{ 0x1a, 1025, 1075},	//1050
	{ 0x2a, 1075, 1125},	//1100
	{ 0x3b, 1125, 1175},	//1150
	{ 0x0b, 1175, 1225},	//1200
	{ 0x1b, 1225, 1275},	//1250
	{ 0x2b, 1275, 1325},	//1300
	{ 0x3c, 1325, 1375},	//1350
	{ 0x0c, 1375, 1425},	//1400
	{ 0x1c, 1425, 1475},	//1450
	{ 0x2c, 1475, 1525},	//1500
	{ 0x3d, 1525, 1575},	//1550
	{ 0x0d, 1575, 1625},	//1600
	{ 0x1d, 1625, 1675},	//1650
	{ 0x2d, 1675, 1725},	//1700
	{ 0x3e, 1725, 1775},	//1750
	{ 0x0e, 1775, 1825},	//1800
	{ 0x1e, 1825, 1875},	//1850
	{ 0x2f, 1875, 1925},	//1900
	{ 0x3f, 1925, 1975},	//1950
	{ 0x0f, 1975, 2025},	//2000
	{ 0x40, 2025, 2075},	//2050
	{ 0x41, 2075, 2125},	//2100
	{ 0x42, 2125, 2175},	//2150
	{ 0x43, 2175, 2225},	//2200
	{ 0x44, 2225, 2275},	//2250
	{ 0x45, 2275, 2325},	//2300
	{ 0x46, 2325, 2375},	//2350
	{ 0x47, 2375, 2425},	//2400
	{ 0x48, 2425, 2475},	//2450
	{ 0x49, 2475, 2500},	//2500
};

static int mipi_csi2_set_phy_freq(struct sdrv_csi_mipi_csi2 *kcmc,
								  uint32_t freq,
								  uint32_t lanes);


static struct v4l2_subdev *mipi_csi2_get_sensor_subdev(
	struct v4l2_subdev *sd)
{
	struct media_pad *sink_pad, *remote_pad;
	int ret;

	ret = sdrv_find_pad(sd, MEDIA_PAD_FL_SINK);
	if (ret < 0)
		return NULL;
	sink_pad = &sd->entity.pads[ret];
	remote_pad = media_entity_remote_pad(sink_pad);
	if (!remote_pad || !is_media_entity_v4l2_subdev(remote_pad->entity))
		return NULL;
	return media_entity_to_v4l2_subdev(remote_pad->entity);
}


void
dw_mipi_csi_write(struct sdrv_csi_mipi_csi2 *kcmc,
				  unsigned int address, unsigned int data)
{
	iowrite32(data, kcmc->base_address + address);
}

u32
dw_mipi_csi_read(struct sdrv_csi_mipi_csi2 *kcmc, unsigned long address)
{
	return ioread32(kcmc->base_address + address);
}


static bool check_version(struct sdrv_csi_mipi_csi2 *kcmc)
{
	u32 version;

	version = dw_mipi_csi_read(kcmc, R_CSI2_VERSION);
	if (version != 0x3133302A) {
		pr_err_ratelimited("mipi_csi2 version error.\n");
		return false;
	} else
		return true;
}

static int dw_enable_mipi_csi_irq_common(struct sdrv_csi_mipi_csi2 *kcmc)
{
	u32 value;
	/*PHY FATAL*/
	value = PHY_ERRSOTSYNCCHS_3|PHY_ERRSOTSYNCCHS_2|PHY_ERRSOTSYNCCHS_1|PHY_ERRSOTSYNCCHS_0;
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_PHY_FATAL, value);
	/*PKT FATAL*/
	value = ERR_ECC_DOUBLE|VC3_ERR_CRC|VC2_ERR_CRC|VC1_ERR_CRC|VC0_ERR_CRC;
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_PKT_FATAL, value);
	/*FRAME FATAL*/
	value = ERR_FRAME_DATA_VC3;
	value |= ERR_FRAME_DATA_VC2;
	value |= ERR_FRAME_DATA_VC1;
	value |= ERR_FRAME_DATA_VC0;
	value |= ERR_F_SEQ_VC3;
	value |= ERR_F_SEQ_VC2;
	value |= ERR_F_SEQ_VC1;
	value |= ERR_F_SEQ_VC0;
	value |= ERR_F_BNDRY_MATCH_VC3;
	value |= ERR_F_BNDRY_MATCH_VC2;
	value |= ERR_F_BNDRY_MATCH_VC1;
	value |= ERR_F_BNDRY_MATCH_VC0;
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_FRAME_FATAL, value);
	/*PHY FATAL*/
	value = PHY_ERRESC_3;
	value |= PHY_ERRESC_2;
	value |= PHY_ERRESC_1;
	value |= PHY_ERRESC_0;
	value |= PHY_ERRSOTHS_3;
	value |= PHY_ERRSOTHS_2;
	value |= PHY_ERRSOTHS_1;
	value |= PHY_ERRSOTHS_0;
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_PHY, value);
	/*IPI FATAL*/
	value = INT_EVENT_FIFO_OVERFLOW;
	value |= PIXEL_IF_HLINE_ERR;
	value |= PIXEL_IF_FIFO_NEMPTY_FS;
	value |= FRAME_SYNC_ERR;
	value |= PIXEL_IF_FIFO_UNDERFLOW;
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_IPI, value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_IPI2, value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_IPI3, value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_IPI4, value);

	return 0;
}

static int dw_disable_mipi_csi_irq_common(struct sdrv_csi_mipi_csi2 *kcmc)
{
	u32 value;
	/*PHY FATAL*/
	value = PHY_ERRSOTSYNCCHS_3|PHY_ERRSOTSYNCCHS_2|PHY_ERRSOTSYNCCHS_1|PHY_ERRSOTSYNCCHS_0;
	value = dw_mipi_csi_read(kcmc, R_CSI2_MASK_INT_PHY_FATAL) & (~value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_PHY_FATAL, value);
	/*PKT FATAL*/
	value = ERR_ECC_DOUBLE|VC3_ERR_CRC|VC2_ERR_CRC|VC1_ERR_CRC|VC0_ERR_CRC;
	value = dw_mipi_csi_read(kcmc, R_CSI2_MASK_INT_PHY_FATAL) & (~value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_PKT_FATAL, value);
	/*FRAME FATAL*/
	value = ERR_FRAME_DATA_VC3;
	value |= ERR_FRAME_DATA_VC2;
	value |= ERR_FRAME_DATA_VC1;
	value |= ERR_FRAME_DATA_VC0;
	value |= ERR_F_SEQ_VC3;
	value |= ERR_F_SEQ_VC2;
	value |= ERR_F_SEQ_VC1;
	value |= ERR_F_SEQ_VC0;
	value |= ERR_F_BNDRY_MATCH_VC3;
	value |= ERR_F_BNDRY_MATCH_VC2;
	value |= ERR_F_BNDRY_MATCH_VC1;
	value |= ERR_F_BNDRY_MATCH_VC0;
	value = dw_mipi_csi_read(kcmc, R_CSI2_MASK_INT_FRAME_FATAL)&(~value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_FRAME_FATAL, value);
	/*PHY FATAL*/
	value = PHY_ERRESC_3;
	value |= PHY_ERRESC_2;
	value |= PHY_ERRESC_1;
	value |= PHY_ERRESC_0;
	value |= PHY_ERRSOTHS_3;
	value |= PHY_ERRSOTHS_2;
	value |= PHY_ERRSOTHS_1;
	value |= PHY_ERRSOTHS_0;
	value = dw_mipi_csi_read(kcmc, R_CSI2_MASK_INT_PHY)&(~value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_PHY, value);
	/*IPI FATAL*/
	value = INT_EVENT_FIFO_OVERFLOW;
	value |= PIXEL_IF_HLINE_ERR;
	value |= PIXEL_IF_FIFO_NEMPTY_FS;
	value |= FRAME_SYNC_ERR;
	value |= PIXEL_IF_FIFO_UNDERFLOW;
	value = dw_mipi_csi_read(kcmc, R_CSI2_MASK_INT_IPI) & (~value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_IPI, value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_IPI2, value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_IPI3, value);
	dw_mipi_csi_write(kcmc, R_CSI2_MASK_INT_IPI4, value);

	return 0;
}


static void mipi_csi2_enable(struct sdrv_csi_mipi_csi2 *kcmc,
							 bool enable)
{
	if (enable) {
		//phy_shutdownz high, not shutdown
		dw_mipi_csi_write(kcmc, R_CSI2_DPHY_SHUTDOWNZ, 1);
		//dphy_rstz high, not reset
		dw_mipi_csi_write(kcmc, R_CSI2_DPHY_RSTZ, 1);
		//resetn high, not reset
		dw_mipi_csi_write(kcmc, R_CSI2_CTRL_RESETN, 1);
		dw_enable_mipi_csi_irq_common(kcmc);
		usleep_range(1000, 2000);
	} else {
		dw_mipi_csi_write(kcmc, R_CSI2_DPHY_SHUTDOWNZ, 0);
		dw_mipi_csi_write(kcmc, R_CSI2_DPHY_RSTZ, 0);
		dw_mipi_csi_write(kcmc, R_CSI2_CTRL_RESETN, 0);
		dw_disable_mipi_csi_irq_common(kcmc);
		usleep_range(1000, 2000);
	}
}


static void set_lane_number(struct sdrv_csi_mipi_csi2 *kcmc, int cnt)
{
	switch (cnt) {
	case 1:
		dw_mipi_csi_write(kcmc, R_CSI2_N_LANES, 0);
		break;
	case 2:
		dw_mipi_csi_write(kcmc, R_CSI2_N_LANES, 1);
		break;
	case 3:
		dw_mipi_csi_write(kcmc, R_CSI2_N_LANES, 2);
		break;
	case 4:
	default:
		dw_mipi_csi_write(kcmc, R_CSI2_N_LANES, 3);
		break;
	}
}

static void set_ipi_mode(struct sdrv_csi_mipi_csi2 *kcmc, int index,
						 int val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_MODE, val);
		break;
	case 2:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI2_MODE, val);
		break;
	case 3:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI3_MODE, val);
		break;
	case 4:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI4_MODE, val);
		break;
	default:
		break;
	}
}

static void set_ipi_vcid(struct sdrv_csi_mipi_csi2 *kcmc, int index)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_VCID, 0);
		break;
	case 2:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI2_VCID, 1);
		break;
	case 3:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI3_VCID, 2);
		break;
	case 4:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI4_VCID, 3);
		break;
	default:
		break;
	}
}

static void set_ipi_data_type(struct sdrv_csi_mipi_csi2 *kcmc, int index,
							  uint8_t type)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_DATA_TYPE, type);
		break;
	case 2:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI2_DATA_TYPE, type);
		break;
	case 3:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI3_DATA_TYPE, type);
		break;
	case 4:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI4_DATA_TYPE, type);
	default:
		break;
	}
}

static void set_ipi_hsa(struct sdrv_csi_mipi_csi2 *kcmc, int index,
						uint32_t val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_HSA_TIME, val);
		break;
	case 2:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI2_HSA_TIME, val);
		break;
	case 3:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI3_HSA_TIME, val);
		break;
	case 4:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI4_HSA_TIME, val);
	default:
		break;
	}
}

static void set_ipi_hbp(struct sdrv_csi_mipi_csi2 *kcmc, int index,
						uint32_t val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_HBP_TIME, val);
		break;
	case 2:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI2_HBP_TIME, val);
		break;
	case 3:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI3_HBP_TIME, val);
		break;
	case 4:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI4_HBP_TIME, val);
	default:
		break;
	}
}

static void set_ipi_hsd(struct sdrv_csi_mipi_csi2 *kcmc, int index,
						uint32_t val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_HSD_TIME, val);
		break;
	case 2:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI2_HSD_TIME, val);
		break;
	case 3:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI3_HSD_TIME, val);
		break;
	case 4:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI4_HSD_TIME, val);
	default:
		break;
	}
}

static void set_ipi_adv_features(struct sdrv_csi_mipi_csi2 *kcmc,
								 int index, uint32_t val)
{
	switch (index) {
	case 1:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_ADV_FEATURES, val);
		break;
	case 2:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI2_ADV_FEATURES, val);
		break;
	case 3:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI3_ADV_FEATURES, val);
		break;
	case 4:
		dw_mipi_csi_write(kcmc, R_CSI2_IPI4_ADV_FEATURES, val);
	default:
		break;
	}
}


static int mipi_csi2_initialization(struct sdrv_csi_mipi_csi2 *kcmc)
{
	int i;

	if (!check_version(kcmc))
		return -ENODEV;
	set_lane_number(kcmc, kcmc->hw.num_lanes);
	for (i = 1; i <= SDRV_MIPI_CSI2_IPI_NUM; i++) {
		set_ipi_mode(kcmc, i, kcmc->hw.ipi_mode);
		set_ipi_vcid(kcmc, i);
		set_ipi_data_type(kcmc, i, kcmc->hw.output_type);
		set_ipi_hsa(kcmc, i, kcmc->hw.hsa);
		set_ipi_hbp(kcmc, i, kcmc->hw.hbp);
		set_ipi_hsd(kcmc, i, kcmc->hw.hsd);
		set_ipi_adv_features(kcmc, i, kcmc->hw.adv_feature);
	}
	mipi_csi2_set_phy_freq(kcmc, kcmc->lanerate / 1000 / 1000,
						   kcmc->lane_count);
	return 0;
}

static int mipi_csi2_s_power(struct v4l2_subdev *sd, int on)
{
#if 0
	struct sdrv_csi_mipi_csi2 *kcmc;
	int ret = 0;
	struct v4l2_subdev *sensor_sd = mipi_csi2_get_sensor_subdev(sd);

	if (!sensor_sd)
		return -EINVAL;
	kcmc = container_of(sd, struct sdrv_csi_mipi_csi2, subdev);
	if (!kcmc)
		return -EINVAL;
	if (kcmc->active_stream_num == 0)
		ret = v4l2_subdev_call(sensor_sd, core, s_power, on);
	return ret;
#else
	return 0;
#endif
}

static int mipi_csi2_set_phy_freq(struct sdrv_csi_mipi_csi2 *kcmc,
								  uint32_t freq,
								  uint32_t lanes)
{
	int phyrate;
	int i;

	phyrate = freq * lanes * 2;
	//phyrate = freq;
	pr_info_ratelimited("%s(): phyrate %dMHz, lanerate=%dMHz\n", __func__,
						phyrate, freq);
	if ((phyrate < g_phy_freqs[0].range_l)
		|| (phyrate > g_phy_freqs[62].range_h)) {
		pr_err_ratelimited("err phy freq\n");
		return -1;
	}
	for (i = 0; i < 63; i++) {
		if ((phyrate >= g_phy_freqs[i].range_l)
			&& (phyrate < g_phy_freqs[i].range_h))
			break;
	}
	kcmc->dispmux_address = devm_ioremap_resource(&kcmc->pdev->dev,
							kcmc->dispmux_res);
	if (IS_ERR(kcmc->dispmux_address)) {
		dev_err(&kcmc->pdev->dev, "dispmux address not right.\n");
		return PTR_ERR(kcmc->dispmux_address);
	}
	if (kcmc->id == 0 || kcmc->id == 2) {
		iowrite32(g_phy_freqs[i].index, kcmc->dispmux_address + 0x60);
	} else if (kcmc->id == 1 || kcmc->id == 3) {
		iowrite32(g_phy_freqs[i].index, kcmc->dispmux_address + 0x68);
	} else
		pr_err_ratelimited("wrong host id\n");
	devm_iounmap(&kcmc->pdev->dev, kcmc->dispmux_address);
	devm_release_mem_region(&kcmc->pdev->dev, kcmc->dispmux_res->start,
							resource_size(kcmc->dispmux_res));
	return 0;
}


static int mipi_csi2_g_frame_interval(struct v4l2_subdev *sd,
									  struct v4l2_subdev_frame_interval *fi)
{
	struct v4l2_subdev *sensor_sd = mipi_csi2_get_sensor_subdev(sd);

	if (!sensor_sd)
		return -EINVAL;
	return v4l2_subdev_call(sensor_sd, video, g_frame_interval, fi);
}

static int mipi_csi2_s_frame_interval(struct v4l2_subdev *sd,
									  struct v4l2_subdev_frame_interval *fi)
{
	struct v4l2_subdev *sensor_sd = mipi_csi2_get_sensor_subdev(sd);

	if (!sensor_sd)
		return -EINVAL;
	return v4l2_subdev_call(sensor_sd, video, s_frame_interval, fi);
}


static int mipi_csi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sdrv_csi_mipi_csi2 *kcmc;
	int ret = 0;
	struct v4l2_subdev *sensor_sd = mipi_csi2_get_sensor_subdev(sd);

	if (!sensor_sd)
		return -EINVAL;
	kcmc = container_of(sd, struct sdrv_csi_mipi_csi2, subdev);
	if (!kcmc)
		return -EINVAL;
	mutex_lock(&kcmc->lock);
	if (enable == 1) {
		if (kcmc->active_stream_num == 0) {
			kcmc->active_stream_num++;
			ret = mipi_csi2_initialization(kcmc);
			if (ret < 0) {
				mutex_unlock(&kcmc->lock);
				return -ENODEV;
			}
			mipi_csi2_enable(kcmc, enable);
			v4l2_subdev_call(sensor_sd, video, s_stream, enable);
		} else
			kcmc->active_stream_num++;
	} else {
		kcmc->active_stream_num--;
		if (kcmc->active_stream_num == 0) {
			mipi_csi2_enable(kcmc, enable);
			v4l2_subdev_call(sensor_sd, video, s_stream, enable);
		}
	}
	mutex_unlock(&kcmc->lock);
	return 0;
}


static int mipi_csi2_enum_mbus_code(struct v4l2_subdev *sd,
									struct v4l2_subdev_pad_config *cfg,
									struct v4l2_subdev_mbus_code_enum *code)
{
	struct v4l2_subdev *sensor_sd = mipi_csi2_get_sensor_subdev(sd);

	if (!sensor_sd)
		return -EINVAL;
	return v4l2_subdev_call(sensor_sd, pad, enum_mbus_code, cfg, code);
}


static int mipi_csi2_get_fmt(struct v4l2_subdev *sd,
							 struct v4l2_subdev_pad_config *cfg,
							 struct v4l2_subdev_format *format)
{
	struct v4l2_subdev *sensor_sd = mipi_csi2_get_sensor_subdev(sd);

	if (!sensor_sd)
		return -EINVAL;
	return v4l2_subdev_call(sensor_sd, pad, get_fmt, cfg, format);
}


static int mipi_csi2_set_fmt(struct v4l2_subdev *sd,
							 struct v4l2_subdev_pad_config *cfg,
							 struct v4l2_subdev_format *format)
{
	struct v4l2_subdev *sensor_sd = mipi_csi2_get_sensor_subdev(sd);

	if (!sensor_sd)
		return -EINVAL;
	return v4l2_subdev_call(sensor_sd, pad, set_fmt, cfg, format);
}


static int mipi_csi2_enum_frame_size(struct v4l2_subdev *sd,
									 struct v4l2_subdev_pad_config *cfg,
									 struct v4l2_subdev_frame_size_enum *fse)
{
	struct v4l2_subdev *sensor_sd = mipi_csi2_get_sensor_subdev(sd);

	if (!sensor_sd)
		return -EINVAL;
	return v4l2_subdev_call(sensor_sd, pad, enum_frame_size, cfg, fse);
}


static int mipi_csi2_enum_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	struct v4l2_subdev *sensor_sd = mipi_csi2_get_sensor_subdev(sd);

	if (!sensor_sd)
		return -EINVAL;
	return v4l2_subdev_call(sensor_sd, pad, enum_frame_interval, cfg, fie);
}



static const struct v4l2_subdev_core_ops sdrv_mipi_csi2_core_ops = {
	.s_power = mipi_csi2_s_power,
};

static const struct v4l2_subdev_video_ops sdrv_mipi_csi2_video_ops = {
	.g_frame_interval = mipi_csi2_g_frame_interval,
	.s_frame_interval = mipi_csi2_s_frame_interval,
	.s_stream = mipi_csi2_s_stream,
};

static const struct v4l2_subdev_pad_ops sdrv_mipi_csi2_pad_ops = {
	.enum_mbus_code = mipi_csi2_enum_mbus_code,
	.get_fmt = mipi_csi2_get_fmt,
	.set_fmt = mipi_csi2_set_fmt,
	.enum_frame_size = mipi_csi2_enum_frame_size,
	.enum_frame_interval = mipi_csi2_enum_frame_interval,
};

static const struct v4l2_subdev_ops sdrv_mipi_csi2_v4l2_ops = {
	.core = &sdrv_mipi_csi2_core_ops,
	.video = &sdrv_mipi_csi2_video_ops,
	.pad = &sdrv_mipi_csi2_pad_ops,
};

static int mipi_csi2_link_setup(struct media_entity *entity,
								const struct media_pad *local,
								const struct media_pad *remote, u32 flags)
{
	if (flags & MEDIA_LNK_FL_ENABLED)
		if (media_entity_remote_pad(local))
			return -EBUSY;
	return 0;
}

static const struct media_entity_operations sdrv_mipi_csi2_media_ops = {
	.link_setup = mipi_csi2_link_setup,
};

static irqreturn_t
dw_mipi_csi_irq(int irq, void *dev_id)
{
	struct sdrv_csi_mipi_csi2 *kcmc = dev_id;
	u32 int_status, ind_status;
	u32 ipi_softrst;
	unsigned long flags;

	int_status = dw_mipi_csi_read(kcmc, R_CSI2_INTERRUPT);
	spin_lock_irqsave(&kcmc->slock, flags);
	if (int_status & CSI2_INT_PHY_FATAL) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_PHY_FATAL);
		pr_info_ratelimited("%LLX CSI INT PHY FATAL: %08X\n",
							kcmc->base_address, ind_status);
	}
	if (int_status & CSI2_INT_PKT_FATAL) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_PKT_FATAL);
		pr_info_ratelimited("%LLX CSI INT PKT FATAL: %08X\n",
							kcmc->base_address, ind_status);
	}
	if (int_status & CSI2_INT_FRAME_FATAL) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_FRAME_FATAL);
		pr_info_ratelimited("%LLX CSI INT FRAME FATAL: %08X\n",
							kcmc->base_address, ind_status);
	}
	if (int_status & CSI2_INT_PHY) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_PHY);
		pr_info_ratelimited("%LLX CSI INT PHY: %08X\n",
							kcmc->base_address, ind_status);
	}
	if (int_status & CSI2_INT_PKT) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_PKT);
		pr_info_ratelimited("%LLX CSI INT PKT: %08X\n",
							kcmc->base_address, ind_status);
	}
	if (int_status & CSI2_INT_LINE) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_LINE);
		pr_info_ratelimited("%LLX CSI INT LINE: %08X\n",
							kcmc->base_address, ind_status);
	}
	if (int_status & CSI2_INT_IPI) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_IPI);
		ipi_softrst = dw_mipi_csi_read(kcmc, R_CSI2_IPI_SOFTRSTN);
		ipi_softrst = ipi_softrst & (~IPI_SOFTRSTN);
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		ipi_softrst |= IPI_SOFTRSTN;
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		pr_info_ratelimited("%LLX CSI INT IPI: %08X\n",
							kcmc->base_address, ind_status);
	}
	if (int_status & CSI2_INT_IPI2) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_IPI2);
		ipi_softrst = dw_mipi_csi_read(kcmc, R_CSI2_IPI_SOFTRSTN);
		ipi_softrst = ipi_softrst & (~IPI2_SOFTRSTN);
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		ipi_softrst |= IPI2_SOFTRSTN;
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		pr_info_ratelimited("%LLX CSI INT IPI: %08X\n",
							kcmc->base_address, ind_status);
	}
	if (int_status & CSI2_INT_IPI3) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_IPI3);
		ipi_softrst = dw_mipi_csi_read(kcmc, R_CSI2_IPI_SOFTRSTN);
		ipi_softrst = ipi_softrst & (~IPI3_SOFTRSTN);
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		ipi_softrst |= IPI3_SOFTRSTN;
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		pr_info_ratelimited("%LLX CSI INT IPI: %08X\n",
							kcmc->base_address, ind_status);
	}
	if (int_status & CSI2_INT_IPI4) {
		ind_status = dw_mipi_csi_read(kcmc, R_CSI2_INT_IPI4);
		ipi_softrst = dw_mipi_csi_read(kcmc, R_CSI2_IPI_SOFTRSTN);
		ipi_softrst = ipi_softrst & (~IPI4_SOFTRSTN);
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		ipi_softrst |= IPI4_SOFTRSTN;
		dw_mipi_csi_write(kcmc, R_CSI2_IPI_SOFTRSTN, ipi_softrst);
		pr_info_ratelimited("%LLX CSI INT IPI: %08X\n",
							kcmc->base_address, ind_status);
	}
	spin_unlock_irqrestore(&kcmc->slock, flags);
	return IRQ_HANDLED;
}

static int
dw_mipi_csi_parse_dt(struct platform_device *pdev,
					 struct sdrv_csi_mipi_csi2 *kcmc)
{
	struct device_node *node = pdev->dev.of_node;
	int reg;
	int ret = 0;

	ret = of_property_read_u32(node, "host_id", &kcmc->id);
	if (ret < 0) {
		dev_err(&pdev->dev, "Missing host id\n");
		return -EINVAL;
	}
	/* Device tree information */
	ret = of_property_read_u32(node, "data-lanes", &kcmc->hw.num_lanes);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read data-lanes\n");
		//return ret;
	}
	if (kcmc->hw.num_lanes == 0)
		kcmc->hw.num_lanes = 4;
	ret = of_property_read_u32(node, "output-type", &kcmc->hw.output_type);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read output-type\n");
		//return ret;
	}
	dev_info(&pdev->dev, "kcmc->hw.output_type=0x%x\n", kcmc->hw.output_type);
	ret = of_property_read_u32(node, "ipi-mode", &kcmc->hw.ipi_mode);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read ipi-mode\n");
		//return ret;
	}
	kcmc->hw.ipi_mode = IPI_MODE_CUT_THROUGH | IPI_MODE_ENABLE;
	kcmc->hw.adv_feature = IPI_ADV_SYNC_LEGACCY | IPI_ADV_USE_VIDEO |
						   IPI_ADV_SEL_LINE_EVENT;
	dev_info(&pdev->dev, "kcmc->hw.ipi_mode=0x%x\n", kcmc->hw.ipi_mode);
	dev_info(&pdev->dev, "kcmc->hw.adv_feature=0x%x\n", kcmc->hw.adv_feature);
	ret =
		of_property_read_u32(node, "ipi-auto-flush",
							 &kcmc->hw.ipi_auto_flush);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read ipi-auto-flush\n");
		//return ret;
	}
	ret =
		of_property_read_u32(node, "ipi-color-mode",
							 &kcmc->hw.ipi_color_mode);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read ipi-color-mode\n");
		//return ret;
	}
	ret =
		of_property_read_u32(node, "virtual-channel", &kcmc->hw.virtual_ch);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read virtual-channel\n");
		//return ret;
	}
	ret = of_property_read_u32(node, "lanerate", &kcmc->lanerate);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read lanerate\n");
		//return ret;
	}
	dev_info(&pdev->dev, "kcmc->lanerate=%d\n", kcmc->lanerate);
	ret = of_property_read_u32(node, "hsa", &kcmc->hw.hsa);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read lanerate\n");
		//return ret;
	}
	dev_info(&pdev->dev, "kcmc->hw.hsa=%d\n", kcmc->hw.hsa);
	ret = of_property_read_u32(node, "hbp", &kcmc->hw.hbp);
	if (ret) {
		dev_info(&pdev->dev, "Couldn't read lanerate\n");
		//return ret;
	}
	dev_info(&pdev->dev, "kcmc->hw.hbp=%d\n", kcmc->hw.hbp);
	ret = of_property_read_u32(node, "hsd", &kcmc->hw.hsd);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't read lanerate\n");
		//return ret;
	}
	dev_info(&pdev->dev, "kcmc->hw.hsd=%d\n", kcmc->hw.hsd);
	node = of_get_child_by_name(node, "port");
	if (!node)
		return -EINVAL;
	ret = of_property_read_u32(node, "reg", &reg);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't read reg value\n");
		//return ret;
	}
	kcmc->index = reg;
	ret = of_property_read_u32(node, "lanerate", &kcmc->lanerate);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't read lanerate\n");
		//return ret;
	}
	dev_info(&pdev->dev, "kcmc->lanerate=%d\n", kcmc->lanerate);
	if (kcmc->index >= CSI_MAX_ENTITIES)
		return -ENXIO;
	return 0;
}
static const struct of_device_id sdrv_mipi_csi2_dt_match[] = {
	{.compatible = "semidrive,sdrv-csi-mipi"},
	{},
};
static int sdrv_mipi_csi2_probe(struct platform_device *pdev)
{
//	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	struct sdrv_csi_mipi_csi2 *kcmc;
	struct v4l2_subdev *sd;
	int ret;
	struct resource *res = NULL;

	dev_info(dev, "%s\n", __func__);
	kcmc = devm_kzalloc(dev, sizeof(*kcmc), GFP_KERNEL);
	if (!kcmc)
		return -ENOMEM;
	kcmc->dev = dev;
	sd = &kcmc->subdev;
	mutex_init(&kcmc->lock);
	spin_lock_init(&kcmc->slock);
	kcmc->pdev = pdev;
	kcmc->active_stream_num = 0;
	//of_id = of_match_node(sdrv_mipi_csi2_dt_match, dev->of_node);
	//if (WARN_ON(of_id == NULL))
	//    return -EINVAL;
	ret = dw_mipi_csi_parse_dt(pdev, kcmc);
//  if (ret < 0)
//	  return ret;
	dev_info(dev, "Request DPHY\n");
	kcmc->phy = devm_phy_get(dev, "csi2-dphy");
//  if (IS_ERR(kcmc->phy))
//	  return PTR_ERR(kcmc->phy);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	kcmc->base_address = devm_ioremap_resource(dev, res);
	if (IS_ERR(kcmc->base_address)) {
		dev_err(dev, "base address not right.\n");
		return PTR_ERR(kcmc->base_address);
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
#if 0
	kcmc->dispmux_address = devm_ioremap_resource(dev, res);
	if (IS_ERR(kcmc->dispmux_address)) {
		dev_err(dev, "dispmux address not right.\n");
		return PTR_ERR(kcmc->dispmux_address);
	}
#else
	kcmc->dispmux_res = res;
#endif
	kcmc->ctrl_irq_number = platform_get_irq(pdev, 0);
	if (kcmc->ctrl_irq_number <= 0) {
		dev_err(dev, "IRQ number not set.\n");
		return kcmc->ctrl_irq_number;
	}
	ret = devm_request_irq(dev, kcmc->ctrl_irq_number,
						   dw_mipi_csi_irq, IRQF_SHARED,
						   dev_name(dev), kcmc);
	if (ret) {
		dev_err(dev, "IRQ failed\n");
		goto error_entity_cleanup;
	}
	v4l2_subdev_init(sd, &sdrv_mipi_csi2_v4l2_ops);
	sd->owner = THIS_MODULE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sprintf(sd->name, "%s", SDRV_MIPI_CSI2_NAME);
	kcmc->fmt = &dw_mipi_csi_formats[0];
	kcmc->format.code = dw_mipi_csi_formats[0].code;
	v4l2_set_subdevdata(sd, kcmc);
#if 1
	kcmc->pads[SDRV_MIPI_CSI2_VC0_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	kcmc->pads[SDRV_MIPI_CSI2_VC1_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	kcmc->pads[SDRV_MIPI_CSI2_VC2_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	kcmc->pads[SDRV_MIPI_CSI2_VC3_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	kcmc->pads[SDRV_MIPI_CSI2_VC0_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;
	kcmc->pads[SDRV_MIPI_CSI2_VC1_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;
	kcmc->pads[SDRV_MIPI_CSI2_VC2_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;
	kcmc->pads[SDRV_MIPI_CSI2_VC3_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;
#else
	kcmc->pads[SDRV_MIPI_CSI2_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	kcmc->pads[SDRV_MIPI_CSI2_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;
#endif
	sd->dev = dev;
	sd->entity.function = MEDIA_ENT_F_IO_V4L;
	sd->entity.ops = &sdrv_mipi_csi2_media_ops;
	ret = media_entity_pads_init(&sd->entity,
								 SDRV_MIPI_CSI2_PAD_NUM, kcmc->pads);
	if (ret < 0) {
		dev_err(dev, "Failed to init media entity:%d\n", ret);
		return ret;
	}
	platform_set_drvdata(pdev, kcmc);
	ret = v4l2_async_register_subdev(sd);
	if (ret < 0) {
		dev_err(dev, "Failed to register async subdev");
		goto error_entity_cleanup;
	}
	return 0;
error_entity_cleanup:
	media_entity_cleanup(&sd->entity);
	return ret;
}

static int sdrv_mipi_csi2_remove(struct platform_device *pdev)
{
#if 1
	struct sdrv_csi_mipi_csi2 *kcmc = platform_get_drvdata(pdev);
	struct v4l2_subdev *sd = &kcmc->subdev;

	media_entity_cleanup(&sd->entity);
#endif
	return 0;
}

MODULE_DEVICE_TABLE(of, sdrv_mipi_csi2_dt_match);

static struct platform_driver sdrv_mipi_csi2_driver = {
	.probe = sdrv_mipi_csi2_probe,
	.remove = sdrv_mipi_csi2_remove,
	.driver = {
		.name = SDRV_MIPI_CSI2_NAME,
		.of_match_table = sdrv_mipi_csi2_dt_match,
	},
};

module_platform_driver(sdrv_mipi_csi2_driver);

#ifdef CONFIG_ARCH_SEMIDRIVE_V9
extern int register_sideb_poc_driver(void);

static const struct of_device_id sdrv_mipi_csi2_sideb_dt_match[] = {
	{.compatible = "semidrive,sdrv-csi-mipi-sideb"},
	{},
};

MODULE_DEVICE_TABLE(of, sdrv_mipi_csi2_sideb_dt_match);

static struct platform_driver sdrv_mipi_csi2_sideb_driver = {
	.probe = sdrv_mipi_csi2_probe,
	.remove = sdrv_mipi_csi2_remove,
	.driver = {
		.name = SDRV_MIPI_CSI2_NAME_SIDEB,
		.of_match_table = sdrv_mipi_csi2_sideb_dt_match,
	},
};

static int __init sdrv_mipi_csi2_sideb_init(void)
{
	int ret;

	ret = register_sideb_poc_driver();
	if (ret < 0) {
		printk("fail to register poc driver for sideb!\n");
	}
	ret = platform_driver_register(&sdrv_mipi_csi2_sideb_driver);
	if (ret < 0) {
		printk("fail to register sideb mipi csi2 driver ret = %d.\n", ret);
	}
	return ret;
}

late_initcall(sdrv_mipi_csi2_sideb_init);
#endif

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive mipi-csi2 driver");
MODULE_LICENSE("GPL v2");
