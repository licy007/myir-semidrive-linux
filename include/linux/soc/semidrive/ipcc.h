
/*
 * Copyright (c) 2019  Semidrive
 */

#ifndef __IPCC_HEAD_FILE__
#define __IPCC_HEAD_FILE__

#include <linux/poll.h>

typedef enum {
	COMM_MSG_INVALID = 0,
	COMM_MSG_CORE,
	COMM_MSG_RPCALL,
	COMM_MSG_DISPLAY,
	COMM_MSG_CAN,
	COMM_MSG_TOUCH,
	COMM_MSG_BACKLIGHT,
	COMM_MSG_STORAGE,
	COMM_MSG_POWER,
	COMM_MSG_CAMERA,
	COMM_MSG_LOGGER,
	COMM_MSG_DEBUGGER,
	COMM_MSG_PROPERTY,
	COMM_MSG_SVCMGR,
	/* Communication Control Message CMD_ID */
	COMM_MSG_CCM_BASE = 0xA0,
	COMM_MSG_CCM_ECHO = 0xA5,
	COMM_MSG_CCM_ACK  = 0xA6,
	COMM_MSG_CCM_DROP = 0xAA,
	COMM_MSG_MAX,
} dcf_msg_type;

/* DCF message definition
 * msg base header is 4 B long
 * msg extention header is 4 bytes indicated by opflag
 * msg body 0 ~ 4K-1 bytes data filled by use case
 */
struct dcf_msg_extend {
	union {
		u32 dat32;
		struct {
			u8 src; 	/* app-id for source, 0 if not used */
			u8 dst; 	/* app-id for destination, 0 if not used */
		} addr;
	} u;
} __attribute__((__packed__));

struct dcf_message {
	u32 msg_type : 8;	/* dcf_msg_type */
	/* Optional features
		SHORT: 1 msg without body
		FRAGMENT: 1
		SYNC: 1
		ACK:  1
		CKSM: 1, checksum exist in the end of packet
	*/
	u32 opflags  : 8;
	u32 msg_len  : 12;	/* msg body len */
	u32 reserved : 4;	/* future use */
	struct dcf_msg_extend option[1];  /* msg option if exist indicated by flag */
	u8 data[0];			/* payload start */
} __attribute__((__packed__));

#define DCF_MSGF_STD		(0x00)
#define DCF_MSGF_OPT		(0x01)
#define DCF_MSGF_SHT		(0x02)
#define DCF_MSGF_FRA		(0x04)
#define DCF_MSGF_SYN		(0x08)
#define DCF_MSGF_ACK		(0x10)
#define DCF_MSGF_CKS		(0x20)

#define DCF_MSG_SIZE(msg)	(msg->msg_len)
#define DCF_MSG_DATA(msg, type)	((type *)&msg->data[0])
#define DCF_MSG_TYPE(msg) 	(msg->msg_type)
#define DCF_MSG_HLEN		(sizeof(struct dcf_message))

#define DCF_RPC_MAX_PARAMS	(8)
#define DCF_RPC_MAX_RESULT	(4)
#define DCF_RPC_REQLEN		((IPCC_RPC_MAX_PARAMS + 4)*sizeof(u32))

struct rpc_req_msg {
	struct dcf_message hdr;
	u32 cmd;
	u32 param[DCF_RPC_MAX_PARAMS];
};

struct rpc_ret_msg {
	struct dcf_message hdr;
	u32 ack;
	u32 retcode;
	u32 result[DCF_RPC_MAX_RESULT];
};

#define DCF_MSG_INIT_HDR(msg, type, len, flags) \
		({msg->msg_type = type; \
		msg->opflags  = flags; \
		msg->msg_len  = len; \
		msg->option[0].u.dat32 = 0x0;})

#define DCF_INIT_RPC_HDR(msg, sz) \
		DCF_MSG_INIT_HDR((msg), COMM_MSG_RPCALL, \
		sz, DCF_MSGF_STD)

#define DCF_INIT_RPC_REQ(req, op) \
		({DCF_INIT_RPC_HDR(&(req).hdr, 4); \
		(req).cmd = op;})

#define DCF_INIT_RPC_REQ1(req, op, param1) \
		({DCF_INIT_RPC_HDR(&(req).hdr, 8); \
		(req).cmd = op; \
		(req).param[0] = param1;})

#define DCF_INIT_RPC_REQ4(req, op, param1, param2, param3, param4) \
		({DCF_INIT_RPC_HDR(&req.hdr, 20); \
		(req).cmd = op;		  \
		(req).param[0] = param1; \
		(req).param[1] = param2; \
		(req).param[2] = param3; \
		(req).param[3] = param4;})

#define DCF_RPC_PARAM(req, type) (type *)(&req.param[0])

/* type:
 * COMM_MSG_CCM_* header
 */
struct dcf_ccm_hdr {
	struct dcf_message	dmsg;
	u32		seq;
	u32		time[4];
	char	data[];
};

/*
 * Display DC status value
 */
typedef enum {
	DC_STAT_NOTINIT     = 0,	/* not initilized by remote cpu */
	DC_STAT_INITING     = 1,	/* on initilizing */
	DC_STAT_INITED      = 2,	/* initilize compilete, ready for display */
	DC_STAT_BOOTING     = 3,	/* during boot time splash screen */
	DC_STAT_CLOSING     = 4,	/* DC is going to close */
	DC_STAT_CLOSED      = 5,	/* DC is closed safely */
	DC_STAT_NORMAL      = 6,	/* DC is used by DRM */
	DC_STAT_MAX         = DC_STAT_NORMAL,
} dc_state_t;

/*
 * I2C status value
 */
typedef enum {
	I2C_STAT_NOTINIT     = 0,	/* not initilized by remote cpu */
	I2C_STAT_INITING     = 1,	/* on initilizing */
	I2C_STAT_INITED      = 2,	/* initilize compilete, ready for i2c */
	I2C_STAT_BOOTING     = 3,	/* during boot time splash screen */
	I2C_STAT_CLOSING     = 4,	/* I2C is going to close */
	I2C_STAT_CLOSED      = 5,	/* I2C is closed safely */
	I2C_STAT_NORMAL      = 6,	/* I2C is used by linux */
	I2C_STAT_MAX         = I2C_STAT_NORMAL,
} i2c_state_t;

#define SD_LOOPBACK_EPT		(12)
#define SD_PROPERTY_EPT		(13)
#define SD_CLUSTER_EPT		(70)
#define SD_IVI_EPT		(71)
#define SD_SSYSTEM_EPT		(72)
#define SD_EARLYAPP_EPT	(80)
/* 101-112 may reserve for vircan devices */
#define SD_VIRCAN0_EPT (101)
#define SD_VIRCAN1_EPT (102)
#define SD_VIRCAN2_EPT (103)
#define SD_VIRCAN3_EPT (104)

/* 130-133 may reserve for virtual i2s*/
#define SD_VIRI2S_EPT (130)

#define SYS_RPC_REQ_BASE			(0x2000)
#define SYS_RPC_REQ_SET_PROPERTY	(SYS_RPC_REQ_BASE + 0)
#define SYS_RPC_REQ_GET_PROPERTY	(SYS_RPC_REQ_BASE + 1)
/* Here defined backlight RPC cmd */
#define MOD_RPC_REQ_BASE			(0x3000)
#define MOD_RPC_REQ_BL_IOCTL		(MOD_RPC_REQ_BASE + 8)
/* Safe Touchscreen ioctl RPC cmd */
#define MOD_RPC_REQ_STS_IOCTL		(MOD_RPC_REQ_BASE + 10)
/* Here defined display controller cmd */
#define MOD_RPC_REQ_DC_IOCTL		(MOD_RPC_REQ_BASE + 16)
/* Safe camera ioctl RPC cmd */
#define MOD_RPC_REQ_SCS_IOCTL		(MOD_RPC_REQ_BASE + 24)
/* Audio service ioctl RPC cmd */
#define MOD_RPC_REQ_AUDIO_IOCTL		(MOD_RPC_REQ_BASE + 32)
/* Safe CAN proxy ioctl RPC cmd */
#define MOD_RPC_REQ_CP_IOCTL		(MOD_RPC_REQ_BASE + 0x40)

int semidrive_rpcall(struct rpc_req_msg *req, struct rpc_ret_msg *result);
int semidrive_get_property(u32 id, u32 *val);
int semidrive_set_property(u32 id, u32 val);
int semidrive_set_property_new(u32 id, u32 val);
int semidrive_get_property_new(u32 id, u32 *val);

int semidrive_send(void *ept, void *data, int len);
int semidrive_trysend(void *ept, void *data, int len);
int semidrive_poll(void *ept, struct file *filp, poll_table *wait);

int sd_close_dc(bool is_block);

bool sd_is_dc_closed(void);

int sd_wait_dc_init(unsigned int timeout_ms);

bool sd_is_dc_inited(void);

int sd_set_dc_status(dc_state_t val);

int sd_get_dc_status(dc_state_t *val);

int sd_kick_vdsp(void);

int sd_get_i2c_status(i2c_state_t *val);
int sd_set_i2c_status(i2c_state_t val);

typedef void(*vdsp_isr_callback)(void *ctx, void *mssg);
int sd_connect_vdsp(void *hwctx, vdsp_isr_callback isr_cb);

#endif //__IPCC_HEAD_FILE__
