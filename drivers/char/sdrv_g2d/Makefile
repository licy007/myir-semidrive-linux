# SPDX-License-Identifier: GPL-2.0
ccflags-y += -g -O1 \
	-DKO_VERSION=\"$(shell date +%Y-%m-%d-%H:%M:%S)\"
# add flag '-pg' when compile g2d lib
ccflags-y += -I$(obj) -I$(obj)/hwconfig -I$(obj)/hwconfig/pipes

sdrv-g2d-objs := \
	sdrv_g2d.o \
	g2d_core.o \
	g2d_sysfs.o \
	hwconfig/g2d_r0p1.o \
	hwconfig/g2dlite_r0p1.o \
	hwconfig/pipes/spipe_g2d.o \
	hwconfig/pipes/gpipe_g2d.o

obj-$(CONFIG_G2D_SEMIDRIVE) += sdrv-g2d.o

