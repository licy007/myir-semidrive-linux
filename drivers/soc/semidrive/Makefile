
sd-xenipc-$(CONFIG_XEN)	+= xenipc-front.o xenipc-back.o rpcall-front.o rpcall-back.o
obj-$(CONFIG_SEMIDRIVE_IPCC)	+= ipcc_rpc.o $(sd-xenipc-y) \
					ipcc_rpmsg_sample.o \
					rpcall.o \
					dc_negotiate.o \
					i2c_negotiate.o \
					vdsp.o \
					dcf_virnet.o \
					chrdev.o \
					chrdev-front.o \
					sdrv_property_client_list.o \
					sdrv_property_client.o

obj-$(CONFIG_SEMIDRIVE_SCR)	+= sdrv_scr.o
obj-$(CONFIG_SEMIDRIVE_VCE)	+= sdrv_vce.o
obj-$(CONFIG_SEMIDRIVE_PFM)	+= sdrv_ddr_pfm.o
obj-$(CONFIG_ARCH_SEMIDRIVE)	+= sdrv_boardinfo_hwid.o sys_counter.o sdrv_extra_gpr.o
obj-$(CONFIG_SEMIDRIVE_SDPE_RPC) += sdpe_rpc.o
obj-$(CONFIG_SUSPEND) += str_sync.o
obj-$(CONFIG_SEMIDRIVE_ULINK) += ulink_init.o ulink_channel.o ulink_call.o ulink_pcie.o ulink_irq.o
obj-$(CONFIG_SEMIDRIVE_LOGBUF)	+= logbuf/
obj-$(CONFIG_ARCH_SEMIDRIVE)	+= sdrv-bufq.o
obj-$(CONFIG_ARCH_SEMIDRIVE)	+= virtio_frontend.o
obj-$(CONFIG_ARCH_SEMIDRIVE)	+= virtio_service.o virtio_provider.o
obj-$(CONFIG_SEMIDRIVE_REGCTL)	+= sdrv_regctl.o
obj-$(CONFIG_SEMIDRIVE_CRASHDUMP_ELF)	+= ramdump/
obj-$(CONFIG_SEMIDRIVE_TIME_SYNC)       += time_sync.o

# add ipcc-rpc config for compatible with MP & safety communication
ifeq ($(IPCC_RPC_RPROC_MP), true)
ccflags-y += -DIPCC_RPC_RPROC_MP
endif

ifeq ($(SDRV_NEW_PROPERTY), true)
ccflags-y += -DSDRV_NEW_PROPERTY
endif
