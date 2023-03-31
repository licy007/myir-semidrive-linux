# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9310_ap2_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9340_ap1_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9342_ap1_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9350_ap1_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9350_ap2_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9360_ap1_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9368_ap1_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9340_ap1_android_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9360_ap1_android_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_D9) += d9_d9350_ap1_android_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_V9) += v9156_ms_native_linux_serdes.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_V9) += v9150_ms_native_linux_serdes.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_V9) += v9752_a_ref.dtb
dtb-$(CONFIG_ARCH_MYIR_SEMIDRIVE_V9) += v9752_b_ref.dtb
#subdir-y	+= archive

always		:= $(dtb-y)
subdir-y	+= overlays
clean-files	:= *.dtb

# Add __symbols__ node
ifeq ($(CONFIG_ARCH_SEMIDRIVE),y)
	DTC_FLAGS ?= -@
endif
