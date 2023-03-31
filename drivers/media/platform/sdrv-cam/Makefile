sdrv-csi-objs := sdrv-core.o sdrv-stream.o sdrv-stream-video.o

obj-$(CONFIG_VIDEO_SEMIDRIVE) += sdrv-csi.o

obj-$(CONFIG_VIDEO_SEMIDRIVE_PARALLEL) += sdrv-parallel.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_MIPICSI2) += sdrv-mipi-csi2.o

obj-$(CONFIG_VIDEO_SEMIDRIVE_SRV) += sdrv-dummy.o

obj-$(CONFIG_VIDEO_SEMIDRIVE_OV5640) += ov5640.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_MAX9286_MAX96705) += max9286_max96705.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_MAX96722_MAX96705) += max96722_max96705.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_MAX96706_MAX96705) += max96706_max96705.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_AR0132_AP0101) += ar0132_ap0101.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_NEXTCHIP_N4) += n4.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_NEXTCHIP_TP2850) += tp2850.o

sdrv-deser-objs := sdrv-desers.o deser_max96722_mipi.o deser_max96706_dvp.o deser_max9286_mipi.o deser_max9296_mipi.o
obj-$(CONFIG_VIDEO_SEMIDRIVE_DESERS_ADAPTIVE) += sdrv-deser.o
obj-$(CONFIG_POWER_POC_DRIVER) += poc-max2008x.o

