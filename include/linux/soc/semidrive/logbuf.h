#ifndef _SD_LOGBUF_H_
#define _SD_LOGBUF_H_
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>

#define SDRV_LOGBUG_SEGMENT_NUM		(2)
#define SDRV_LOGBUF_BANK_HEADER_SIZE	0x40

#define CLUSTER_AP1_ID			(1)
#define CLUSTER_AP2_ID			(2)
#define CLUSTER_INV_ID			(10)

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

typedef enum {
	SAFETY_R5 = 0,
	SECURE_R5,
	MPC_R5,
	AP1_A55,
	AP1_USER,
	AP2_A55,
	AP2_USER,
	LOG_TYPE_MAX
} log_type_t;

/*
 * to keep the following structure alignment, we need follow
 * points below:
 * - use architecture-independment types (eg: int, char)
 * - use single-byte or four-byte alignment
 */
/* reserved 64 byte to describe per bank header */
typedef union logbuf_bk_hdr {
	struct logbuf_bk_header {
		uint32_t tail;      /* read  counter, used by sdshell */
		uint32_t tail1;
		uint32_t head;      /* write counter */
		uint32_t size;      /* bank buffer size */
		uint32_t linepos;   /* pos value corresponding to '\n' */
		uint32_t flag;
	} header;
	uint8_t pad[SDRV_LOGBUF_BANK_HEADER_SIZE];
} logbuf_bk_hdr_t;

typedef struct logbuf_bk_desc {
	bool dump_log_en;
	char last_char;
	uint32_t last_head;
	const char *name;
	unsigned long flags;
	logbuf_bk_hdr_t *hdr;
	spinlock_t lock;
} logbuf_bk_desc_t;

typedef struct sdlog_msg {
	uint32_t type;
	uint32_t size;
	uint8_t data[0];
} sdlog_msg_t;


/* buffer interface */
int sdrv_logbuf_init(void);
int sdrv_logbuf_early_init(void);
logbuf_bk_desc_t *sdrv_logbuf_get_bank_desc(log_type_t typ);
int sdrv_logbuf_putc_with_timestamp(logbuf_bk_desc_t *desc, char c, bool stamp);
void sdrv_logbuf_clear_buf(logbuf_bk_desc_t *desc);
char *sdrv_logbuf_get_last_buf(logbuf_bk_desc_t *desc);
int sdrv_logbuf_get_bank_buf_size(logbuf_bk_desc_t *desc);
int sdrv_logbuf_get_new_log_size(logbuf_bk_desc_t *desc);
int sdrv_logbuf_get_last_buf_and_size(logbuf_bk_desc_t *desc, char *buf,
				      int len, bool upd_tail1);
int sdrv_logbuf_get_new_buf_and_size(logbuf_bk_desc_t *desc, char *buf,
				     int len, bool upd_tail1);
bool sdrv_logbuf_has_initialized(void);

int sdrv_logbuf_copy_to_user(sdlog_msg_t *msg, void __user *arg, char *buf,
			     int len);
struct regmap *sdrv_logbuf_regmap_get(void);

#endif /* _SD_LOGBUF_H_ */
