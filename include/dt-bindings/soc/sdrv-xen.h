/* semidrive virtulization specific dt bindings */
#ifndef __DT_BINDINGS_SDRV_XEN_H
#define __DT_BINDINGS_SDRV_XEN_H

/* sid max:0x7f */

#define SDRV_SID_CRTC0        0x01
#define SDRV_SID_CRTC1        0x02
#define SDRV_SID_CRTC2        0x03
#define SDRV_SID_CRTC3        0x04

#define SDRV_SID_G2D1         0x05
#define SDRV_SID_G2D2         0x06

#define SDRV_SID_DMAC2        0x07
#define SDRV_SID_DMAC3        0x08
#define SDRV_SID_DMAC4        0x09
#define SDRV_SID_DMAC5        0x0A
#define SDRV_SID_DMAC6        0x0B
#define SDRV_SID_DMAC7        0x0C

#define SDRV_SID_SDHCI3       0x0D

#define SDRV_SID_USB0         0x0E
#define SDRV_SID_USB1         0x0F

#define SDRV_SID_GPU0         0x10
#define SDRV_SID_GPU0_0       0x11
#define SDRV_SID_GPU0_1       0x12
#define SDRV_SID_GPU0_2       0x13
#define SDRV_SID_GPU0_3       0x14

#define SDRV_SID_GPU1         0x20
#define SDRV_SID_GPU1_0       0x21
#define SDRV_SID_GPU1_1       0x22
#define SDRV_SID_GPU1_2       0x23
#define SDRV_SID_GPU1_3       0x24

#define SDRV_SID_ETH1         0x30
#define SDRV_SID_ETH2         0x31

#define SDRV_SID_VPU1         0x32
#define SDRV_SID_VPU2         0x33


#define SDRV_SID_CSI0         0x34
#define SDRV_SID_CSI1         0x35
#define SDRV_SID_CSI2         0x36

/* 0x40 - 0x4F */
#define SDRV_SID_PCIE0        0x40
/* 0x50 - 0x57 */
#define SDRV_SID_PCIE1        0x50


/* vm dtb */
#define GUEST_PHANDLE_GIC (65000)

/* xen vm reserved memory */
#define XEN_RESERVED_BASE              0x90000000

#define XEN_RESERVED_VM_BASE           (XEN_RESERVED_BASE+0x00)
#define XEN_RESERVED_VM_SIZE           0x02000000

#define XEN_RESERVED_VDSP_BASE         (XEN_RESERVED_BASE + XEN_RESERVED_VM_SIZE)
#define XEN_RESERVED_VDSP_SIZE         0x04000000

#define XEN_RESERVED_SERVICE_BASE      (XEN_RESERVED_BASE + XEN_RESERVED_VM_SIZE + XEN_RESERVED_VDSP_SIZE)
#define XEN_RESERVED_SERVICE_SIZE      0x05200000

#endif
