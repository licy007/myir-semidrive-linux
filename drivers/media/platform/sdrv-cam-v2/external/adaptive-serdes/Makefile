ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/common
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/mipicsi
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/csi-controller
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/external/sensor
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/external/serdes
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/external/adaptive-serdes
ccflags-y += -Idrivers/media/platform/sdrv-cam-v2/external/poc

ccflags-y += -D__LINUX__

sdrv-deser-objs := sdrv-desers.o deser_max96722_mipi.o deser_max96706_dvp.o deser_max9286_mipi.o deser_max9296_mipi.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_DESERS_ADAPTIVE) += sdrv-deser.o

