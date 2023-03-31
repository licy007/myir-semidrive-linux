ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/common/
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/mipicsi
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/parallel
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/csi-controller
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/external/sensor
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/external/serdes

ccflags-y += -D__LINUX__

obj-$(CONFIG_VIDEO_SEMIDRIVE_V2) += common/

obj-$(CONFIG_VIDEO_SEMIDRIVE_V2) += mipicsi/
obj-$(CONFIG_VIDEO_SEMIDRIVE_V2) += parallel/
obj-$(CONFIG_VIDEO_SEMIDRIVE_V2) += csi-controller/
obj-$(CONFIG_VIDEO_SEMIDRIVE_V2) += external/
obj-$(CONFIG_VIDEO_SEMIDRIVE_V2) += sdrv-cam-video.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_V2) += sdrv-cam-core.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_SRV) += sdrv-cam-safety-client.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_V2) += sdrv-cam-debug.o