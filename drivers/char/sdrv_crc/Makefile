# SPDX-License-Identifier: GPL-2.0
ccflags-y += -g -O1 \
	-DCRC_VERSION=\"$(shell date +%Y-%m-%d-%H:%M:%S)\"
# add flag '-pg' when compile g2d lib
ccflags-y += -I$(obj)

sdrv-disp-crc-objs := \
	sdrv_crc.o \
	dc_crc_ops.o

obj-$(CONFIG_DISP_CRC_SEMIDRIVE) += sdrv-disp-crc.o
