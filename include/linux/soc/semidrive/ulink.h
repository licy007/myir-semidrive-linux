
/*
 * Copyright (c) 2021  Semidrive
 */

#ifndef __ULINK_HEAD_FILE__
#define __ULINK_HEAD_FILE__

#define CALL_PARA_MAX 24

typedef enum {
	DP_CR5_SAF,
	DP_CR5_SEC,
	DP_CR5_MPC,
	DP_CA_AP1,
	DP_CA_AP2,
	DP_DSP_V,
	DP_CPU_MAX,
	R_DP_CR5_SAF = DP_CPU_MAX,
	R_DP_CR5_SEC,
	R_DP_CR5_MPC,
	R_DP_CA_AP1,
	R_DP_CA_AP2,
	R_DP_DSP_V,
	ALL_DP_CPU_MAX,
} domain_cpu_id_t;

typedef enum {
	DIR_PUSH,//from local to remote
	DIR_PULL,//from remote to local
} pcie_dir;

struct ulink_channel{
	uint32_t rproc;
	uint32_t addr_sub;
	void (*callback)(struct ulink_channel *ch, uint8_t *data, uint32_t size);
	void *user_data;
};

struct ulink_call_msg {
	uint32_t size;
	uint8_t data[CALL_PARA_MAX];
};

struct ulink_call {
	struct ulink_channel *ch;
	struct mutex call_mutex;
};

typedef void (*ulink_rx_callback)(struct ulink_channel *ch, uint8_t *data, uint32_t size);

int ulink_pcie_init(void);
void ulink_pcie_exit(void);
int ulink_irq_init(void);
void ulink_irq_exit(void);
int ulink_channel_init(void);
void ulink_channel_exit(void);

struct ulink_channel* ulink_request_channel(uint32_t rproc, uint32_t addr_sub, ulink_rx_callback f);
void ulink_release_channel(struct ulink_channel *ch);
int ulink_channel_send(struct ulink_channel* ch, const char *data, uint32_t size);
uint32_t ulink_channel_rev(struct ulink_channel* ch, uint8_t **data);

struct ulink_call *ulink_request_call(uint32_t rproc, uint32_t addr_sub);
void ulink_release_call(struct ulink_call *call);
uint32_t ulink_call(struct ulink_call *call, struct ulink_call_msg *smsg, struct ulink_call_msg *rmsg);

void ulink_trigger_irq(uint32_t irq_num);
void ulink_request_irq(uint32_t irq_num, void(*func)(void *), void *data);

int kunlun_pcie_dma_copy(uint64_t local, uint64_t remote, uint32_t len, pcie_dir dir);
#endif //_ULINK_HEAD_FILE__
