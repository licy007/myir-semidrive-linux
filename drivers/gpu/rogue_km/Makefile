########################################################################### ###
#@Title         Root kernel makefile
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
#
# The contents of this file are subject to the MIT license as set out below.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
#
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
#
# This License is also included in this distribution in the file called
# "MIT-COPYING".
#
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

# This top-level kbuild makefile builds all the Linux kernel modules in the
# DDK. To run kbuild, this makefile is copied to $(TARGET_PRIMARY_OUT)/kbuild/Makefile
# and make is invoked in $(TARGET_PRIMARY_OUT)/kbuild.

# This makefile doesn't define any kbuild special variables apart from
# ccflags-y and obj-m. The variables for objects are picked up by including
# the kbuild makefile fragments named in $(INTERNAL_KBUILD_MAKEFILES). The
# list of objects that these fragments make is collected in
# $(INTERNAL_KBUILD_OBJECTS) and $(INTERNAL_EXTRA_KBUILD_OBJECTS). These
# variables are set according to the build's $(KERNEL_COMPONENTS) and
# $(EXTRA_PVRSRVKM_COMPONENTS). To add a new kernel module to the build, edit
# these variables in the per-build Makefile.
TOP := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
include $(TOP)/config_kernel.mk
PVRSRVKM_NAME := $(PVRSRV_MODNAME)
HWDEFS_DIR := $(TOP)/hwdefs/$(PVR_ARCH)
BRIDGE_SOURCE_ROOT   := $(TOP)/generated/$(PVR_ARCH)
bridge_base := $(BRIDGE_SOURCE_ROOT)

ifeq ($(CONFIG_ANDROID), y)
ccflags-y += -DANDROID
endif

ifeq ($(CONFIG_BUFFER_SYNC), y)
ccflags-y += -DSUPPORT_BUFFER_SYNC
endif

ifeq ($(cc-name), clang)
ccflags-y += -Wno-error=typedef-redefinition -Wno-error=enum-conversion
endif

ifeq ($(CONFIG_PM_DEVFREQ),y)
ifeq ($(CONFIG_DEVFREQ_GOV_USERSPACE),y)
ccflags-y += -DSUPPORT_LINUX_DVFS
else ifeq ($(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND),y)
ccflags-y += -DSUPPORT_LINUX_DVFS
endif
endif

ccflags-y += -DPVR_BUILD_DIR=\"$(PVR_BUILD_DIR)\"
ccflags-y += -DRGX_NUM_OS_SUPPORTED=$(RGX_NUM_OS_SUPPORTED)
ccflags-y += -DVMM_TYPE=$(VMM_TYPE)
ccflags-y += -DPVR_SYSTEM=$(PVR_SYSTEM)
ccflags-y += -DPVR_SYSTEM_CLEAN=$(PVR_SYSTEM_CLEAN)
ccflags-y += -D__linux__ -include $(TOP)/config_kernel.h \
 -include kernel_config_compatibility.h \
 -I$(TOP)/include -I$(TOP)/kernel/drivers/staging/imgtec

include $(TOP)/services/server/env/linux/Kbuild.mk

ifneq ($(KERNEL_DRIVER_DIR),)
 ccflags-y += \
   -I$(abspath $(srctree))/$(KERNEL_DRIVER_DIR)/$(PVR_SYSTEM) \
   -I$(abspath $(srctree))/$(KERNEL_DRIVER_DIR)
endif

$(if $($(PVRSRVKM_NAME)-y),,$(warning $(PVRSRVKM_NAME)-y was empty, which could mean that srvkm is missing from $$(KERNEL_COMPONENTS)))
$(PVRSRVKM_NAME)-y += $(foreach _m,$(INTERNAL_EXTRA_KBUILD_OBJECTS:.o=),$($(_m)-y))

obj-m += $(PVRSRVKM_NAME).o
