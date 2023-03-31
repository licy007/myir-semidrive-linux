#below configs come from .config
override PVR_SYSTEM := kl_gm9446
override PVR_SYSTEM_CLEAN := 9446
ifeq ($(CONFIG_VMM_TYPE),"stub")
override VMM_TYPE := stub
else
override VMM_TYPE := xen
endif
ifneq ($(CONFIG_PVRSRV_VZ_NUM_OSID),)
override RGX_NUM_OS_SUPPORTED := $(CONFIG_PVRSRV_VZ_NUM_OSID)
else
override RGX_NUM_OS_SUPPORTED := 1
endif

ifeq ($(CONFIG_BUFFER_SYNC),y)
override SUPPORT_BUFFER_SYNC := 1
endif

ifeq ($(CONFIG_ION),y)
override PVRSRV_ENABLE_PVR_ION_STATS := 1
override SUPPORT_ION := 1
endif

ifeq ($(CONFIG_ANDROID),y)
override SUPPORT_ANDROID_PLATFORM := 1
endif

ifeq ($(CONFIG_PM_DEVFREQ),y)
ifeq ($(CONFIG_DEVFREQ_GOV_USERSPACE),y)
override SUPPORT_LINUX_DVFS := 1
else ifeq ($(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND),y)
override SUPPORT_LINUX_DVFS := 1
endif
endif

override PVR_BUILD_TYPE := release
override BUILD := release

override PVR_ARCH_DEFS := rogue
override PVRSRV_DIR := services
override HOST_PRIMARY_ARCH := host_x86_64
override HOST_32BIT_ARCH := host_i386
override HOST_FORCE_32BIT := -m32
override HOST_ALL_ARCH := host_x86_64 host_i386
override TARGET_PRIMARY_ARCH := target_aarch64
override TARGET_SECONDARY_ARCH := target_armv7-a
override TARGET_ALL_ARCH := target_aarch64 target_armv7-a
override TARGET_FORCE_32BIT :=
override PVR_ARCH := rogue
override METAG_VERSION_NEEDED := 2.8.1.0.3
override MIPS_VERSION_NEEDED := 2014.07-1
override RISCV_VERSION_NEEDED := 1.0.1
override KERNEL_COMPONENTS := srvkm
override PVRSRV_MODNAME := pvrsrvkm
override PVR_BUILD_TYPE := release
override SUPPORT_RGX := 1
override PVR_LOADER :=
override USE_PVRSYNC_DEVNODE := 1
override BUILD := release
override SORT_BRIDGE_STRUCTS := 1
override DEBUGLINK := 1
override COMPRESS_DEBUG_SECTIONS := 1
override SUPPORT_PHYSMEM_TEST := 1
override SUPPORT_MIPS_64K_PAGE_SIZE :=
override SUPPORT_POWMON_COMPONENT := 1
override OPTIM := -O2
override RGX_TIMECORR_CLOCK := sched
override PDVFS_COM_HOST := 1
override PDVFS_COM_AP := 2
override PDVFS_COM_PMC := 3
override PDVFS_COM_IMG_CLKDIV := 4
override PDVFS_COM := PDVFS_COM_HOST
override PVR_GPIO_MODE_GENERAL := 1
override PVR_GPIO_MODE_POWMON_PIN := 2
override PVR_GPIO_MODE := PVR_GPIO_MODE_GENERAL
override PVR_HANDLE_BACKEND := idr
override SUPPORT_DMABUF_BRIDGE := 1
override SUPPORT_USC_BREAKPOINT := 1
override SUPPORT_DI_BRG_IMPL := 1
override SUPPORT_NATIVE_FENCE_SYNC := 1
override SUPPORT_DMA_FENCE := 1
override RK_INIT_V2 := 1
