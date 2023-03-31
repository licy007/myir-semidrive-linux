# SPDX-License-Identifier: GPL-2.0
ccflags-y += -g -O1 -DKO_VERSION=\"$(shell date +%Y-%m-%d-%H:%M:%S)\"
ccflags-y += -I$(obj) -I$(obj)/planes -I$(obj)/connector


sdrv-drm-objs := \
  drm_drv.o \
  drm_fb.o \
  drm_gem.o \
  drm_memblock.o

dsi-objs := \
	dsi/dwc_mipi_dsi_host.o \
	dsi/sdrv-mipi-dsi.o

drm-crtc-objs += \
	sdrv_dpc.o \
	sdrv_plane.o \
	sdrv_rpcall.o \
	sdrv_unilink.o \
	connector/parallel_interface.o \
	connector/lvds_interface.o \
	connector/lvds_config.o \
	planes/dp_dummy.o \
	planes/dp_rpcall.o \
	planes/dp_r0p1.o \
	planes/dc_r0p1.o \
	planes/gpipe_dc.o \
	planes/spipe.o \
	planes/apipe.o \
	planes/gpipe_dp_s.o \
	planes/gpipe_dp.o \
	planes/dp_unilink.o

obj-$(CONFIG_DRM_SEMIDRIVE) += drm-crtc.o drm_fbdev.o
obj-$(CONFIG_DRM_SEMIDRIVE) += drm_connector.o
obj-$(CONFIG_DRM_SEMIDRIVE) += sdrv-drm.o
obj-$(CONFIG_DRM_SEMIDRIVE) += dsi.o
obj-$(CONFIG_DRM_SEMIDRIVE_VIDEO_DISPLAY) += sdrv_simu_disp.o

