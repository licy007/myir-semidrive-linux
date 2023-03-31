#ifndef _UAPI_LINUX_VIRTIO_MMC_H
#define _UAPI_LINUX_VIRTIO_MMC_H
/* This header, excluding the #ifdef __KERNEL__ part, is BSD licensed so
 * anyone can use the definitions to implement compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */

/* Virtio devices use a standardized configuration space to define their
 * features and pass configuration information, but each implementation can
 * store and access that space differently. */
#include <linux/types.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>

#define VIRTIO_MMC_CDB_DEFAULT_SIZE  32
#define VIRTIO_MMC_VIRTQUEUE_SIZE   16

/* Feature Bits */
#define VIRTIO_MMC_F_CAPABILITY     (1<<0)
#define VIRTIO_MMC_F_ERR_RECOVERY   (1<<1)
#define VIRTIO_MMC_F_CQE            (1<<2)
#define VIRTIO_MMC_F_RO	            (1<<3)
#define VIRTIO_MMC_F_BLK_SIZE       (1<<4)
#define VIRTIO_MMC_F_W_CACHE        (1<<5)
#define VIRTIO_MMC_F_FLUSH          (1<<6)
#define VIRTIO_MMC_F_RETUNING       (1<<7)
#define VIRTIO_MMC_F_HOTPLUG        (1<<8)

/* mmc cmd descriptor type */
#define VIRTIO_MMC_CDB_SDHCI 0
#define VIRTIO_MMC_CDB_CQHCI 1

/* mmc cmd descriptor buf mode */
#define VIRTIO_MMC_SDHCI_BUF_NORMAL	            0
#define VIRTIO_MMC_SDHCI_BUF_ADDR               1
#define VIRTIO_MMC_SDHCI_BUF_AMDA2_32BITS_DESC  2
#define VIRTIO_MMC_SDHCI_BUF_AMDA2_64BITS_DESC  3
#define VIRTIO_MMC_SDHCI_BUF_AMDA3_DESC	        4

/* Response status codes of sdhci host shi*/
#define VIRTIO_MMC_SDHCI_S_OK                   0
#define VIRTIO_MMC_SDHCI_S_CMD_TOUT_ERR         (1<<0)
#define VIRTIO_MMC_SDHCI_S_CMD_CRC_ERR          (1<<1)
#define VIRTIO_MMC_SDHCI_S_CMD_END_BIT_ERR      (1<<2)
#define VIRTIO_MMC_SDHCI_S_CMD_IDX_ERR          (1<<3)
#define VIRTIO_MMC_SDHCI_S_DATA_TOUT_ERR        (1<<4)
#define VIRTIO_MMC_SDHCI_S_DATA_CRC_ERR         (1<<5)
#define VIRTIO_MMC_SDHCI_S_DATA_END_BIT_ERR     (1<<6)
#define VIRTIO_MMC_SDHCI_S_CUR_LMT_ERR          (1<<7)
#define VIRTIO_MMC_SDHCI_S_AUTO_CMD_ERR         (1<<8)
#define VIRTIO_MMC_SDHCI_S_AMDA_ERR             (1<<9)
#define VIRTIO_MMC_SDHCI_S_TUNING_ERR           (1<<10)
#define VIRTIO_MMC_SDHCI_S_RESP_ERR             (1<<11)
#define VIRTIO_MMC_SDHCI_S_BOOT_ACK_ERR         (1<<12)
#define VIRTIO_MMC_SDHCI_S_UNSUPP               (1<<31)

/* Events.  */
#define VIRTIO_MMC_T_EVENTS_MISSED            0x80000000
#define VIRTIO_MMC_T_NO_EVENT                 0
#define VIRTIO_MMC_T_TRANSPORT_RESET          1
// #define VIRTIO_MMC_T_ASYNC_NOTIFY             2
// #define VIRTIO_MMC_T_PARAM_CHANGE             3

/* Reasons of transport reset event */
#define VIRTIO_MMC_EVT_RESET_HARD             0
#define VIRTIO_MMC_EVT_RESET_RESCAN           1
#define VIRTIO_MMC_EVT_RESET_REMOVED          2

enum part_access_type {
	PART_ACCESS_DEFAULT = 0x0,
	PART_ACCESS_BOOT1,
	PART_ACCESS_BOOT2,
	PART_ACCESS_RPMB,
	PART_ACCESS_GP1,
	PART_ACCESS_GP2,
	PART_ACCESS_GP3,
	PART_ACCESS_GP4
};

struct virtio_mmc_config {
	u8 dev_id;
	u8 dev_type;
	u8 part_id;
	u8 reserve01;
	u32 sector_size;
	u32 sector;
	u32 num_sectors;
} __attribute__ ((__packed__));

struct virtio_mmc_sdhci_cmd_desc {
	u32 sector;
	u32 num_sectors;
	u16 cmd_index;     /* Command index */
	u16 cmd_timeout;   /* Command timeout in ms */
	u32 argument;      /* Command argument */
	u8 resp_type;      /* Response type of the command */
	u8 cmd_retry;      /* Retry the command, if card is busy */
	u8 cmd23_support;  /* If card supports cmd23 */
	u8 part_type;      /* emmc hw partition */
	u8 buf_mode;
} __attribute__ ((__packed__));

/* mmc command request, followed by data-out */
struct virtio_mmc_cmd_req {
	u64 id;        /* Command identifier */
	u16 priority;   /* command priority field */
	u16 cdb_type;   /* mmc cmd descriptor type */
	u32 CDB[VIRTIO_MMC_CDB_DEFAULT_SIZE];   /* mmc cmd descriptor data */
} __attribute__ ((__packed__));

/* Response, followed by sense data and data-in */
struct virtio_mmc_cmd_resp {
	u32 resp[4];   /* 128 bit response value */
	u32 status;     /* Command completion status */
} __attribute__ ((__packed__));

struct virtio_mmc_event {
	u32 event;
	u32 reason;
};

struct virtio_mmc_req {
	struct virtio_mmc_cmd_req *cmd;
	void *req_data;
	uint32_t req_data_len;
	struct virtio_mmc_cmd_resp *cmd_resp;
};

#endif /* _UAPI_LINUX_VIRTIO_MMC_H */
