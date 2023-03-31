#pragma once

#define DOMAIN_OFFSET			0x10000000

/* 256 bytes mmio reg and 256 bytes cfg area */
#define MMC_VIRTIO_REG_SIZE		0x200
/* request queue and status queue */
#define MMC_VIRTIO_VQ_SIZE		0x3600

#define MMC_VIRTIO_TOTAL_SIZE		(MMC_VIRTIO_REG_SIZE+MMC_VIRTIO_VQ_SIZE)

/* saf <-> ap1 */
#define MMC_VIRTIO_BASE_AP1		(SAF_AP1_MEMBASE+DOMAIN_OFFSET+SAF_AP1_MEMSIZE-MMC_VIRTIO_TOTAL_SIZE)
#define MMC_VIRTIO_REG_BASE_AP1		(MMC_VIRTIO_BASE_AP1)
#define MMC_VIRTIO_VQ_BASE_AP1		(MMC_VIRTIO_REG_BASE_AP1+MMC_VIRTIO_REG_SIZE)
#define MMC_VIRTIO_MBOX_ADDR_AP1	0xD0
#define MMC_VIRTIO_MBOX_ADDR2_AP1	0xD000

/* saf <-> ap2 */
#define MMC_VIRTIO_BASE_AP2		(SAF_AP2_MEMBASE+DOMAIN_OFFSET+SAF_AP2_MEMSIZE-MMC_VIRTIO_TOTAL_SIZE)
#define MMC_VIRTIO_REG_BASE_AP2		(MMC_VIRTIO_BASE_AP2)
#define MMC_VIRTIO_VQ_BASE_AP2		(MMC_VIRTIO_REG_BASE_AP2+MMC_VIRTIO_REG_SIZE)
#define MMC_VIRTIO_MBOX_ADDR_AP2	0xD1
#define MMC_VIRTIO_MBOX_ADDR2_AP2	0xD100

/*************************VPU_CODA988*************************/
#define VPU_CODA988_FRONT_DEV_RESERV_REG_SIZE (MMC_VIRTIO_TOTAL_SIZE)

/* 256 bytes mmio reg and 256 bytes cfg area */
#define VPU_CODA988_VIRTIO_REG_SIZE 0x200
/* 32(16 tx & 16 rx) number of request queue and status queue */
#define VPU_CODA988_VIRTIO_VQ_SIZE 0x400

#define VPU_CODA988_VIRTIO_TOTAL_SIZE (VPU_CODA988_VIRTIO_REG_SIZE + VPU_CODA988_VIRTIO_VQ_SIZE)

/* saf <-> ap1 */
#define VPU_CODA988_VIRTIO_BASE_AP1                                                                \
    (SAF_AP1_MEMBASE + DOMAIN_OFFSET + SAF_AP1_MEMSIZE - VPU_CODA988_VIRTIO_TOTAL_SIZE -           \
     VPU_CODA988_FRONT_DEV_RESERV_REG_SIZE)
#define VPU_CODA988_VIRTIO_REG_BASE_AP1 (VPU_CODA988_VIRTIO_BASE_AP1)
#define VPU_CODA988_VIRTIO_VQ_BASE_AP1                                                             \
    (VPU_CODA988_VIRTIO_REG_BASE_AP1 + VPU_CODA988_VIRTIO_REG_SIZE)
#define VPU_CODA988_VIRTIO_MBOX_ADDR_AP1 0xD4
#define VPU_CODA988_VIRTIO_MBOX_ADDR2_AP1 0xD400

/* saf <-> ap2 */
#define VPU_CODA988_VIRTIO_BASE_AP2                                                                \
    (SAF_AP2_MEMBASE + DOMAIN_OFFSET + SAF_AP2_MEMSIZE - VPU_CODA988_VIRTIO_TOTAL_SIZE -           \
     VPU_CODA988_FRONT_DEV_RESERV_REG_SIZE)
#define VPU_CODA988_VIRTIO_REG_BASE_AP2 (VPU_CODA988_VIRTIO_BASE_AP2)
#define VPU_CODA988_VIRTIO_VQ_BASE_AP2                                                             \
    (VPU_CODA988_VIRTIO_REG_BASE_AP2 + VPU_CODA988_VIRTIO_REG_SIZE)
#define VPU_CODA988_VIRTIO_MBOX_ADDR_AP2 0xD5
#define VPU_CODA988_VIRTIO_MBOX_ADDR2_AP2 0xD500

/*************************VPU_WAVE412*************************/
#define VPU_WAVE412_FRONT_DEV_RESERV_REG_SIZE (MMC_VIRTIO_TOTAL_SIZE + VPU_CODA988_VIRTIO_TOTAL_SIZE)

/* 256 bytes mmio reg and 256 bytes cfg area */
#define VPU_WAVE412_VIRTIO_REG_SIZE 0x200
/* 32(16 tx & 16 rx) number of request queue and status queue */
#define VPU_WAVE412_VIRTIO_VQ_SIZE 0x400

#define VPU_WAVE412_VIRTIO_TOTAL_SIZE (VPU_WAVE412_VIRTIO_REG_SIZE + VPU_WAVE412_VIRTIO_VQ_SIZE)

/* saf <-> ap1 */
#define VPU_WAVE412_VIRTIO_BASE_AP1                                                                \
    (SAF_AP1_MEMBASE + DOMAIN_OFFSET + SAF_AP1_MEMSIZE - VPU_WAVE412_VIRTIO_TOTAL_SIZE -           \
     VPU_WAVE412_FRONT_DEV_RESERV_REG_SIZE)
#define VPU_WAVE412_VIRTIO_REG_BASE_AP1 (VPU_WAVE412_VIRTIO_BASE_AP1)
#define VPU_WAVE412_VIRTIO_VQ_BASE_AP1                                                             \
    (VPU_WAVE412_VIRTIO_REG_BASE_AP1 + VPU_WAVE412_VIRTIO_REG_SIZE)
#define VPU_WAVE412_VIRTIO_MBOX_ADDR_AP1 0xD6
#define VPU_WAVE412_VIRTIO_MBOX_ADDR2_AP1 0xD600

/* saf <-> ap2 */
#define VPU_WAVE412_VIRTIO_BASE_AP2                                                                \
    (SAF_AP2_MEMBASE + DOMAIN_OFFSET + SAF_AP2_MEMSIZE - VPU_WAVE412_VIRTIO_TOTAL_SIZE -           \
     VPU_WAVE412_FRONT_DEV_RESERV_REG_SIZE)
#define VPU_WAVE412_VIRTIO_REG_BASE_AP2 (VPU_WAVE412_VIRTIO_BASE_AP2)
#define VPU_WAVE412_VIRTIO_VQ_BASE_AP2                                                             \
    (VPU_WAVE412_VIRTIO_REG_BASE_AP2 + VPU_WAVE412_VIRTIO_REG_SIZE)
#define VPU_WAVE412_VIRTIO_MBOX_ADDR_AP2 0xD7
#define VPU_WAVE412_VIRTIO_MBOX_ADDR2_AP2 0xD700

/*************************VPU_WAVE521*************************/
#define VPU_WAVE521_FRONT_DEV_RESERV_REG_SIZE                                                      \
    (MMC_VIRTIO_TOTAL_SIZE + VPU_CODA988_VIRTIO_TOTAL_SIZE + VPU_WAVE412_VIRTIO_TOTAL_SIZE)

/* 256 bytes mmio reg and 256 bytes cfg area */
#define VPU_WAVE521_VIRTIO_REG_SIZE 0x200
/* 32(16 tx & 16 rx) number of request queue and status queue */
#define VPU_WAVE521_VIRTIO_VQ_SIZE 0x400

#define VPU_WAVE521_VIRTIO_TOTAL_SIZE (VPU_WAVE521_VIRTIO_REG_SIZE + VPU_WAVE521_VIRTIO_VQ_SIZE)

/* saf <-> ap1 */
#define VPU_WAVE521_VIRTIO_BASE_AP1                                                                \
    (SAF_AP1_MEMBASE + DOMAIN_OFFSET + SAF_AP1_MEMSIZE - VPU_WAVE521_VIRTIO_TOTAL_SIZE -           \
     VPU_WAVE521_FRONT_DEV_RESERV_REG_SIZE)
#define VPU_WAVE521_VIRTIO_REG_BASE_AP1 (VPU_WAVE521_VIRTIO_BASE_AP1)
#define VPU_WAVE521_VIRTIO_VQ_BASE_AP1                                                             \
    (VPU_WAVE521_VIRTIO_REG_BASE_AP1 + VPU_WAVE521_VIRTIO_REG_SIZE)
#define VPU_WAVE521_VIRTIO_MBOX_ADDR_AP1 0xD8
#define VPU_WAVE521_VIRTIO_MBOX_ADDR2_AP1 0xD800

/* saf <-> ap2 */
#define VPU_WAVE521_VIRTIO_BASE_AP2                                                                \
    (SAF_AP2_MEMBASE + DOMAIN_OFFSET + SAF_AP2_MEMSIZE - VPU_WAVE521_VIRTIO_TOTAL_SIZE -           \
     VPU_WAVE521_FRONT_DEV_RESERV_REG_SIZE)
#define VPU_WAVE521_VIRTIO_REG_BASE_AP2 (VPU_WAVE521_VIRTIO_BASE_AP2)
#define VPU_WAVE521_VIRTIO_VQ_BASE_AP2                                                             \
    (VPU_WAVE521_VIRTIO_REG_BASE_AP2 + VPU_WAVE521_VIRTIO_REG_SIZE)
#define VPU_WAVE521_VIRTIO_MBOX_ADDR_AP2 0xD9
#define VPU_WAVE521_VIRTIO_MBOX_ADDR2_AP2 0xD900