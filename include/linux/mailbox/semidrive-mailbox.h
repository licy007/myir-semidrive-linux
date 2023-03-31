/*
 * Copyright (c) 2019~2020  Semidrive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _LINUX_SEMIDRIVE_MB_H_
#define _LINUX_SEMIDRIVE_MB_H_

#include <xen/interface/io/ring.h>
#include <xen/interface/grant_table.h>
#include <linux/mailbox_controller.h>
#include <linux/soc/semidrive/mb_msg.h>

#define MB_MAX_RPROC	(8)

#ifdef CONFIG_SEMIDRIVE_TIME_SYNC
#define MB_TIME_SYNC_CHANS    (4)
#else
#define MB_TIME_SYNC_CHANS    (0)
#endif

#ifdef CONFIG_SEMIDRIVE_ULINK
#define MB_ULINK_CHANS    (4)
#else
#define MB_ULINK_CHANS    (0)
#endif

#define MB_BASE_CHANS    (16)
#define MB_MAX_CHANS    (MB_BASE_CHANS + MB_TIME_SYNC_CHANS + MB_ULINK_CHANS)

#define MB_MAX_MSGS		(4)
#define MB_MAX_NAMES	(16)
#define MB_BUF_LEN		(4096)
#define MB_BANK_LEN		(MB_BUF_LEN/MB_MAX_MSGS)

#define MB_MSG_REGs_PER_MSG     (3U)
#define MB_MSG_REGs_PER_CPU     (MB_MSG_REGs_PER_MSG * 4)
#define MB_MSGID_INVALID        (0xff)
#define MB_ADDR_ANY             (0xdeadceefU)
#define MB_TMH_RESET_VAL        0x02U

#define MB_CHAN_FLAG_EARLY_ACK	BIT(0)

#define MU_MASTERID_OFF         0x500U
#define MU_TX_BUF_BASE          0x1000U
#define MU_TX_BUF_SIZE          0x1000U
#define MU_RX_BUF_BASE          0x2000U
#define MU_RX_BUF_SIZE          MU_TX_BUF_SIZE
#define MU_RX_BUF_OFF(cpu)      (MU_RX_BUF_SIZE * (cpu))

#define GRANT_INVALID_REF	0

#define CONFIG_MBOX_DUMP_HEX	(0)

enum master_id {
	MASTER_SAF_PLATFORM	= 0,
	MASTER_SEC_PLATFORM	= 1,
	MASTER_MP_PLATFORM	= 2,
	MASTER_AP1			= 3,
	MASTER_AP2			= 4,
	MASTER_VDSP			= 5,
};

struct sd_mbox_tx_msg {
	bool	used;
	unsigned msg_id;
	unsigned length;
	bool	 use_buffer;
	unsigned remote;
	void __iomem *tmh;
	void __iomem *tmc;
	void __iomem *tx_buf;
};

#define FM_TMH0_MDP	(0xffU << 16U)
#define FV_TMH0_MDP(v) \
	(((v) << 16U) & FM_TMH0_MDP)

#define FM_TMH0_TXMES_LEN	(0x7ffU << 0U)
#define FV_TMH0_TXMES_LEN(v) \
	(((v) << 0U) & FM_TMH0_TXMES_LEN)

#define FM_TMH0_MID	(0xffU << 24U)
#define FV_TMH0_MID(v) \
	(((v) << 24U) & FM_TMH0_MID)

#define BM_TMH0_TXUSE_MB	(0x01U << 11U)

#define FM_TMH0_MBM	(0xfU << 12U)
#define FV_TMH0_MBM(v) \
	(((v) << 12U) & FM_TMH0_MBM)

#ifndef addr_t
typedef void __iomem * addr_t;
#endif

struct sd_mbox_chan {
	char chan_name[MB_MAX_NAMES];
	struct sd_mbox_tx_msg *msg;
	unsigned target;
	unsigned actual_size;
	unsigned protocol;
	bool priority;
	bool is_run;
	void __iomem *rmh;
	u32 cl_data;
	u32 dest_addr;
	int quota; /* for balance between chan */
	u32 flags;
};

typedef struct sd_mbox_device {
	void __iomem *reg_base;
	void __iomem *txb_base;
	void __iomem *rxb_base;
	int curr_cpu;
	int irq;
	spinlock_t msg_lock;
	struct sd_mbox_tx_msg tmsg[MB_MAX_MSGS];
	struct sd_mbox_chan mlink[MB_MAX_CHANS];
	struct mbox_chan chan[MB_MAX_CHANS];
	struct mbox_controller controller;
	bool initialized;
	struct device *dev;
} sd_mbox_controller_t;

/* mailbox virtualization */
#define XEN_MBOX_DEVICE_ID	"vmbox"

/* command */
enum {
	MBC_NEW_CHANNEL = 1,
	MBC_DEL_CHANNEL = 2,
};

struct mbox_request_data {
	uint32_t req_id; /* private to guest, echoed in response */
	uint32_t cmd;    /* command to execute */
	u8 osid;
	u8 mba;
};

#define MU_TYPE_NEWDATA	(0x1)
#define MU_TYPE_RACK	(0x2)

struct mbox_respond_data {
	uint32_t req_id; /* private to guest, echoed in response */
	uint32_t result;    /* command to execute */
};

struct mbox_data_head {
	u8 origin_proc;
	u8 src_addr;
	u8 dst_addr;
	u8 type;
	char data[0];
} __attribute__((__packed__));

DEFINE_RING_TYPES(mbox, struct mbox_request_data, struct mbox_respond_data);

struct mbox_data_intf {
    RING_IDX in_cons, in_prod, in_error;

    uint8_t pad1[52];

    RING_IDX out_cons, out_prod, out_error;

    uint8_t pad2[52];

    RING_IDX ring_order;
    grant_ref_t ref[];
};
DEFINE_XEN_FLEX_RING(mbox);

inline static void copy_from_mbox(void *buf, void *mssg, size_t len)
{
	if(xen_domain() && !xen_initial_domain())
		memcpy(buf, mssg, len);
	else
		memcpy_fromio(buf, mssg, len);
}

bool sd_mu_is_msg_sending(struct sd_mbox_tx_msg *msg);
struct sd_mbox_tx_msg *sd_mu_alloc_msg(struct sd_mbox_device *mbdev, int prefer, bool priority);
void sd_mu_free_msg(struct sd_mbox_device *mbdev, struct sd_mbox_tx_msg *msg);
void sd_mu_send_msg(struct sd_mbox_tx_msg *msg);
#ifdef CONFIG_XEN
void mbox_back_received_data(sd_msghdr_t *data, int remote_proc, int src, int dest);
#else
inline static void mbox_back_received_data(sd_msghdr_t *data, int remote_proc, int src, int dest)
{
}
#endif

#endif /* _LINUX_SEMIDRIVE_MB_H_ */
