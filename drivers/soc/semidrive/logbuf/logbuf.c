#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <asm/cacheflush.h>
#include <linux/io.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>
#include <linux/soc/semidrive/logbuf.h>
#include <linux/soc/semidrive/sys_counter.h>
#include <linux/serial_8250.h>
#include <linux/console.h>
#include <linux/log2.h>
#include "logflag.h"

#define SDRV_LOGBUF_MAGIC		0x20210618
#define SDRV_LOGBUF_HEADER_SIZE		0x80
#define SDRV_LINE_MAX			(1024)

#define SDLOG_BANK_BUF_FLAG_OVERRIDE    BIT(0)

/* reserved 128 byte to describe the logbuf header */
typedef union {
	struct logbuf_header {
		uint32_t magic;
		uint32_t logoff[LOG_TYPE_MAX];
		uint32_t logsize[LOG_TYPE_MAX];
		uint32_t status;
		uint32_t uart_mask;
	} header;
	uint8_t pad[SDRV_LOGBUF_HEADER_SIZE];
} logbuf_hdr_t;

static void __iomem *membase;
static uint32_t memsize;
static bool logbuf_init_flag;
static int logbuf_timestamp;
static int con_log_type = LOG_TYPE_MAX;
static logbuf_bk_desc_t bk_desc[LOG_TYPE_MAX];
static struct device_node *logbuf_node;

static bool sdrv_logbuf_is_valid(void __iomem *addr)
{
	logbuf_hdr_t *hdr = (logbuf_hdr_t *)addr;

	return (hdr->header.magic == SDRV_LOGBUF_MAGIC);
}

static logbuf_bk_hdr_t *sdrv_logbuf_get_bank_hdr(void __iomem *addr,
						 log_type_t type)
{
	logbuf_hdr_t *hdr = (logbuf_hdr_t *)addr;
	logbuf_bk_hdr_t *bk_hdr;

	if ((hdr->header.magic != SDRV_LOGBUF_MAGIC) || (type >= LOG_TYPE_MAX))
		return NULL;

	bk_hdr = (logbuf_bk_hdr_t *)((long)(addr + hdr->header.logoff[type]));

	/* the bank buffer size must be a power of two */
	if (bk_hdr->header.size == 0x0)
		bk_hdr->header.size = hdr->header.logsize[type] -
					SDRV_LOGBUF_BANK_HEADER_SIZE;

	if (!is_power_of_2(bk_hdr->header.size))
		return NULL;

	return (void *)bk_hdr;
}

logbuf_bk_desc_t *sdrv_logbuf_get_bank_desc(log_type_t type)
{
	if (type >= LOG_TYPE_MAX)
		return NULL;

	return &bk_desc[type];
}
EXPORT_SYMBOL(sdrv_logbuf_get_bank_desc);

static inline int sdrv_logbuf_putc(logbuf_bk_hdr_t *hdr, char *buf, char c)
{
	buf[hdr->header.head] = c;
	hdr->header.head++;
	if (hdr->header.head >= hdr->header.size)
		hdr->header.flag |= SDLOG_BANK_BUF_FLAG_OVERRIDE;
	hdr->header.head = hdr->header.head & (hdr->header.size - 1);

	return 0;
}

static inline int sdrv_logbuf_get_timestamp(char *buf, int len)
{
	if (buf)
		return snprintf(buf, len, "[%010llu us] ", semidrive_get_systime_us());

	return 0;
}

static inline void sdrv_logbuf_lock(logbuf_bk_desc_t *desc)
{
	spin_lock_irqsave(&desc->lock, desc->flags);
}

static inline void sdrv_logbuf_unlock(logbuf_bk_desc_t *desc)
{
	spin_unlock_irqrestore(&desc->lock, desc->flags);
}

int sdrv_logbuf_putc_with_timestamp(logbuf_bk_desc_t *desc, char c, bool stamp)
{
	int i;
	int len;
	char timestamp[21] = {0};
	logbuf_bk_hdr_t *bk_hdr = desc->hdr;
	char *buf = (char *)((long)bk_hdr + SDRV_LOGBUF_BANK_HEADER_SIZE);

	if (logbuf_timestamp && stamp && (desc->last_char == '\n') && c) {
		len = sdrv_logbuf_get_timestamp(timestamp, 21);
		sdrv_logbuf_lock(desc);
		for (i = 0; i < len; i++)
			sdrv_logbuf_putc(bk_hdr, buf, timestamp[i]);
		sdrv_logbuf_unlock(desc);
	}

	sdrv_logbuf_lock(desc);
	sdrv_logbuf_putc(bk_hdr, buf, c);
	sdrv_logbuf_unlock(desc);
	desc->last_char = c;

	return 0;
}
EXPORT_SYMBOL(sdrv_logbuf_putc_with_timestamp);

void sdrv_logbuf_clear_buf(logbuf_bk_desc_t *desc)
{
	logbuf_bk_hdr_t *bk_hdr = desc->hdr;
	char *buf = (char *)((long)bk_hdr + SDRV_LOGBUF_BANK_HEADER_SIZE);
	int i = 0;

	sdrv_logbuf_lock(desc);

	do {
		bk_hdr->header.head = 0;
		bk_hdr->header.tail = 0;
		bk_hdr->header.tail1 = 0;
		bk_hdr->header.linepos = 0;
		bk_hdr->header.flag = 0;
		memset(buf, 0x0, bk_hdr->header.size);
	} while ((i++ < 5) && ((bk_hdr->header.head != 0)
			   || (bk_hdr->header.flag != 0)
			   || (bk_hdr->header.tail != 0)
			   || (bk_hdr->header.tail1 != 0)));

	sdrv_logbuf_unlock(desc);
}
EXPORT_SYMBOL(sdrv_logbuf_clear_buf);

int sdrv_logbuf_get_bank_buf_size(logbuf_bk_desc_t *desc)
{
	int size;
	logbuf_bk_hdr_t *bk_hdr = desc->hdr;

	sdrv_logbuf_lock(desc);
	size = bk_hdr->header.size;
	sdrv_logbuf_unlock(desc);

	return size;
}
EXPORT_SYMBOL(sdrv_logbuf_get_bank_buf_size);

int sdrv_logbuf_get_new_log_size(logbuf_bk_desc_t *desc)
{
	logbuf_bk_hdr_t *bk_hdr = desc->hdr;
	int delta;

	sdrv_logbuf_lock(desc);
	if (bk_hdr->header.head >= bk_hdr->header.tail1)
		delta = bk_hdr->header.head - bk_hdr->header.tail1;
	else
		delta = bk_hdr->header.size + bk_hdr->header.head -
				bk_hdr->header.tail1;
	sdrv_logbuf_unlock(desc);

	return delta;
}
EXPORT_SYMBOL(sdrv_logbuf_get_new_log_size);

/*
 * in order to use memcpy improve copy efficiency, we copy the ring buffer in
 * the two segments
 */
static int sdrv_logbuf_get_bank_last_buf(logbuf_bk_hdr_t *bk_hdr, char **buf,
					 int *size, int len)
{
	if (!bk_hdr || !buf || !size)
		return -EINVAL;

	if (len != SDRV_LOGBUG_SEGMENT_NUM)
		return -EINVAL;

	if (bk_hdr->header.flag & SDLOG_BANK_BUF_FLAG_OVERRIDE) {
		buf[0] = (char *)((long)bk_hdr + SDRV_LOGBUF_BANK_HEADER_SIZE +
							bk_hdr->header.head);
		size[0] = bk_hdr->header.size - bk_hdr->header.head;
		buf[1] = (char *)((long)bk_hdr + SDRV_LOGBUF_BANK_HEADER_SIZE);
		size[1] = bk_hdr->header.head;
	} else {
		buf[0] = (char *)((long)bk_hdr + SDRV_LOGBUF_BANK_HEADER_SIZE);
		size[0] = bk_hdr->header.head;
		buf[1] = NULL;
		size[1] = 0;
	}

	return 0;
}

static int sdrv_logbuf_get_bank_new_buf(logbuf_bk_hdr_t *bk_hdr, char **buf,
					int *size, int len)
{
	if (!bk_hdr || !buf || !size)
		return -EINVAL;

	if (len != SDRV_LOGBUG_SEGMENT_NUM)
		return -EINVAL;

	if (bk_hdr->header.head >= bk_hdr->header.tail1) {
		buf[0] = (char *)((long)bk_hdr + SDRV_LOGBUF_BANK_HEADER_SIZE +
						bk_hdr->header.tail1);
		size[0] = bk_hdr->header.head - bk_hdr->header.tail1;
		buf[1] = NULL;
		size[1] = 0;
	} else {
		buf[0] = (char *)((long)bk_hdr + SDRV_LOGBUF_BANK_HEADER_SIZE +
						bk_hdr->header.tail1);
		size[0] = bk_hdr->header.size - bk_hdr->header.tail1;
		buf[1] = (char *)((long)bk_hdr + SDRV_LOGBUF_BANK_HEADER_SIZE);
		size[1] = bk_hdr->header.head;
	}

	return 0;
}

static int sdrv_logbuf_get_buf_and_size(logbuf_bk_desc_t *desc, char *buf,
				      int len, bool upd_tail1, bool last)
{
	logbuf_bk_hdr_t *bk_hdr = desc->hdr;
	char *tmp_buf[SDRV_LOGBUG_SEGMENT_NUM];
	int tmp_size[SDRV_LOGBUG_SEGMENT_NUM];
	int count = 0;
	int i;

	if (!buf)
		return -EINVAL;

	sdrv_logbuf_lock(desc);

	if (last) {
		if (sdrv_logbuf_get_bank_last_buf(bk_hdr, tmp_buf,
					tmp_size, SDRV_LOGBUG_SEGMENT_NUM)) {
			sdrv_logbuf_unlock(desc);
			return -EINVAL;
		}
	} else {
		if (sdrv_logbuf_get_bank_new_buf(bk_hdr, tmp_buf,
					tmp_size, SDRV_LOGBUG_SEGMENT_NUM)) {
			sdrv_logbuf_unlock(desc);
			return -EINVAL;
		}
	}

	if (len < (tmp_size[0] + tmp_size[1])) {
		sdrv_logbuf_unlock(desc);
		return -EINVAL;
	}

	/* used to mark the start position of the next copy */
	if (upd_tail1)
		bk_hdr->header.tail1 = bk_hdr->header.head;

	for (i = 0; i < SDRV_LOGBUG_SEGMENT_NUM; i++) {
		if (tmp_size[i]) {
			memcpy(buf + count, tmp_buf[i], tmp_size[i]);
			count += tmp_size[i];
		}
	}
	sdrv_logbuf_unlock(desc);

	return count;
}
int sdrv_logbuf_get_last_buf_and_size(logbuf_bk_desc_t *desc, char *buf,
				      int len, bool upd_tail1)
{
	return sdrv_logbuf_get_buf_and_size(desc, buf, len, upd_tail1, true);
}
EXPORT_SYMBOL(sdrv_logbuf_get_last_buf_and_size);

/* we copy the ring buffer in the two segments
 *
 * the minimum bank buffer of log is equal to 0x20000 bytes. iit will take
 * about 9s to fill the entire ring buffer if we calculate the printing time
 * according to 115200 baudrate. in order to prevent the log buffer is
 * overwritten again, we should ensure that the refresh time interval is less
 * than 9s. in this driver, we use the macro SDRV_TIMER_PERIOD to define the
 * time interval.
 */
int sdrv_logbuf_get_new_buf_and_size(logbuf_bk_desc_t *desc, char *buf,
				     int len, bool upd_tail1)
{
	return sdrv_logbuf_get_buf_and_size(desc, buf, len, upd_tail1, false);
}
EXPORT_SYMBOL(sdrv_logbuf_get_new_buf_and_size);

int sdrv_logbuf_copy_to_user(sdlog_msg_t *msg, void __user *arg,
			     char *buf, int len)
{
	void __user *uarg = arg;

	if (!buf)
		return -EINVAL;

	if (msg->size < len)
		return -EINVAL;

	msg->size = 0;

	if (copy_to_user((void *)uarg + sizeof(*msg), buf, len))
		return -EFAULT;

	msg->size = len;

	if (copy_to_user((void *)arg, msg, sizeof(*msg)))
		return -EFAULT;

	return 0;
}
EXPORT_SYMBOL(sdrv_logbuf_copy_to_user);

static int sdrv_logbuf_dump(log_type_t type)
{
	char mbuf[SDRV_LINE_MAX] = {0};
	logbuf_bk_desc_t *desc;
	char *buf[SDRV_LOGBUG_SEGMENT_NUM];
	int size[SDRV_LOGBUG_SEGMENT_NUM];
	int k = 0;
	int i;
	int j;

	desc = sdrv_logbuf_get_bank_desc(type);
	if (!desc || !desc->hdr)
		return -EINVAL;

	if (!desc->dump_log_en)
		return 0;

	if (sdrv_logbuf_get_bank_last_buf(desc->hdr, buf, size,
					  ARRAY_SIZE(buf))) {
		pr_err("failed to get buf and size\n");
		return -EINVAL;
	}

	for (i = 0; i < SDRV_LOGBUG_SEGMENT_NUM; i++) {
		for (j = 0; j < size[i]; j++) {
			mbuf[k & (SDRV_LINE_MAX - 1)] = buf[i][j];
			if (mbuf[k & (SDRV_LINE_MAX - 1)] == '\n') {
				printk(KERN_INFO "%s: %s", desc->name, mbuf);
				memset(mbuf, 0x0, SDRV_LINE_MAX);
				k = 0;
			} else {
				k++;
			}
		}
	}

	/* handle the last line which dose not end by '\n' */
	if (mbuf[0] != 0)
		printk(KERN_INFO "%s: %s\n", desc->name, mbuf);

	return 0;
}

static void sdrv_logbuf_dump_all(void)
{
	sdrv_logbuf_dump(SAFETY_R5);
	sdrv_logbuf_dump(SECURE_R5);
}

struct regmap *sdrv_logbuf_regmap_get(void)
{
	int ret;
	struct of_phandle_args args;
	struct device_node *regctl_node;
	struct device *regctl_dev;
	struct regmap *regmap = NULL;

	if (!logbuf_node) {
		pr_err("invalid logbuf node\n");
		return NULL;
	}

	ret = of_parse_phandle_with_args(logbuf_node, "regctl",
			"#regctl-cells", 0, &args);
	if (ret) {
		pr_err("regctl node no found from dts\n");
		return NULL;
	}

	regctl_node = args.np;
	regctl_dev =  regctl_node->data;
	if (!regctl_dev) {
		pr_err("find regctl device failed\n");
		return NULL;
	}
	regmap = dev_get_regmap(regctl_dev, NULL);
	of_node_put(args.np);

	return regmap;
}

static void sdrv_console_uart_disable(struct regmap *regmap)
{
	struct console *c;
	struct uart_driver *drv;
	struct uart_state *state;
	struct uart_port *port;

	for_each_console(c) {
		if (!c->device)
			continue;
		if (!c->write)
			continue;
		if ((c->flags & CON_ENABLED) == 0)
			continue;
		if (strcmp(c->name, "ttyS"))
			continue;

		drv = c->data;
		if (!drv)
			continue;
		state = drv->state + c->index;
		if (!state)
			continue;
		port = state->uart_port;
		if (!port)
			continue;

		pr_info("console name: %s, idx: %d, irq: %d\n", c->name,
			c->index, port->irq);
		disable_irq_nosync(port->irq);
		sdrv_saf_uart_flag_set(regmap, 0x1);
	}
}

static int sdrv_logbuf_panic_handler(struct notifier_block *this,
				     unsigned long event, void *unused)
{
	struct regmap *regmap;

	/*
	 * we dump log when the panic occurs, if the dump time exceeds the
	 * wdt timeout period, the wdt will trigger a system restart (it
	 * causes the cache is not synchronized). to avoid this problem,
	 * either disable log dump feature or set a longer timeout(at least
	 * 22s) for watchdog.
	 */
	sdrv_logbuf_dump_all();  /* disable log dump */
	regmap = sdrv_logbuf_regmap_get();
	if (regmap)
		sdrv_console_uart_disable(regmap);

	return NOTIFY_OK;
}

static struct notifier_block sdrv_logbuf_panic_event_nb = {
	.notifier_call	= sdrv_logbuf_panic_handler,
};

static int sdrv_logbuf_bk_init(const char *name, void __iomem *addr,
			       log_type_t type)
{
	if (type >= LOG_TYPE_MAX)
		return -EINVAL;

	bk_desc[type].last_char = '\n';
	bk_desc[type].name = name;
	bk_desc[type].hdr = sdrv_logbuf_get_bank_hdr(addr, type);
	if (!bk_desc[type].hdr) {
		pr_err("failed to get log buffer bank header with %s\n", name);
		return -ENOMEM;
	}

	spin_lock_init(&bk_desc[type].lock);

	return 0;
}

int sdrv_logbuf_early_init(void)
{
	return -EINVAL;
}

int sdrv_logbuf_early_release(void)
{
	return 0;
}

int sdrv_logbuf_init(void)
{
	struct resource res;
	struct device_node *np;
	logbuf_bk_desc_t *desc;
	int ret = 0;

	desc = sdrv_logbuf_get_bank_desc(AP2_USER);
	if (!desc)
		return -EINVAL;

	/* return if the virtual uart driver has already initialized logbuf */
	if (desc->hdr != NULL)
		return 0;

	np = of_find_compatible_node(NULL, NULL, "semidrive, logbuf");
	if (!np) {
		pr_err("no logbuf node\n");
		return -ENODEV;
	}

	if (!of_device_is_available(np)) {
		pr_warning("%pOF: disabled\n", np);
		return -ENODEV;
	}
	logbuf_node = np;

	if (of_address_to_resource(np, 0, &res))
		return -EINVAL;

	memsize = resource_size(&res);
	membase = ioremap_wc(res.start, memsize);
	if (!membase)
		return -ENOMEM;

	if (!sdrv_logbuf_is_valid(membase)) {
		pr_err("log buffer is invalid\n");
		ret = -EINVAL;
		goto unmap;
	}

	pr_info("logbuf base: 0x%lx, size: 0x%x\n", (unsigned long) membase, memsize);

	ret = sdrv_logbuf_bk_init("safety_r5", membase, SAFETY_R5);
	if (ret)
		goto unmap;

	ret = sdrv_logbuf_bk_init("secure_r5", membase, SECURE_R5);
	if (ret)
		goto unmap;

	ret = sdrv_logbuf_bk_init("mpc_r5", membase, MPC_R5);
	if (ret)
		goto unmap;

	ret = sdrv_logbuf_bk_init("ap2_kernel", membase, AP2_A55);
	if (ret)
		goto unmap;

	ret = sdrv_logbuf_bk_init("ap2_user", membase, AP2_USER);
	if (ret)
		goto unmap;

	ret = sdrv_logbuf_bk_init("ap1_kernel", membase, AP1_A55);
	if (ret)
		goto unmap;

	sdrv_logbuf_early_release();

	logbuf_init_flag = true;

	return 0;
unmap:
	iounmap(membase);

	return ret;
}
EXPORT_SYMBOL(sdrv_logbuf_init);

bool sdrv_logbuf_has_initialized(void)
{
	return logbuf_init_flag;
}
EXPORT_SYMBOL(sdrv_logbuf_has_initialized);

static void logbuf_console_write(struct console *con, const char *s,
				 unsigned int c)
{
	unsigned int i;

	for (i = 0; i < c; i++, s++) {
		if (*s == '\n')
			sdrv_logbuf_putc_with_timestamp(&bk_desc[con_log_type],
							'\r', true);
		sdrv_logbuf_putc_with_timestamp(&bk_desc[con_log_type],
						*s, true);
	}
}

static struct console logbuf_console = {
	.name	= "logbuf",
	.write	= logbuf_console_write,
	.flags	= CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index	= -1,
};

static int sdrv_logbuf_initcall(void)
{
	int ret;

	ret = sdrv_logbuf_init();
	if (ret)
		return ret;

	__inval_dcache_area(membase, memsize);

	if (con_log_type == AP1_A55 || con_log_type == AP2_A55)
		register_console(&logbuf_console);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &sdrv_logbuf_panic_event_nb);

	return 0;
}
core_initcall_sync(sdrv_logbuf_initcall);

#ifndef MODULE
static int sdrv_logbuf_dump_args_setup(char *str, log_type_t type)
{
	logbuf_bk_desc_t *desc;
	unsigned long val;
	int ret;

	desc = sdrv_logbuf_get_bank_desc(type);
	if (!desc)
		return 0;

	ret = kstrtoul(str, 0, &val);
	if (ret)
		return 0;

	desc->dump_log_en = val;

	return 1;
}

static int __init dump_saf_log_setup(char *str)
{
	if (!str)
		return 0;

	return sdrv_logbuf_dump_args_setup(str, SAFETY_R5);
}

__setup("dump_saf_log=", dump_saf_log_setup);

static int __init dump_sec_log_setup(char *str)
{
	if (!str)
		return 0;

	return sdrv_logbuf_dump_args_setup(str, SECURE_R5);
}

__setup("dump_sec_log=", dump_sec_log_setup);

static int __init logbuf_timestamp_setup(char *str)
{
	if (!str)
		return 0;

	logbuf_timestamp = simple_strtoul(str, &str, 0);

	return 1;
}

__setup("logbuf.timestamp=", logbuf_timestamp_setup);

static int __init logbuf_console_setup(char *str)
{
	if (!str)
		return 0;

	if (strncasecmp(str, "=ap1", 4) == 0)
		con_log_type = AP1_A55;
	else if (strncasecmp(str, "=ap2", 4) == 0)
		con_log_type = AP2_A55;
	else
		con_log_type = LOG_TYPE_MAX;

	return 1;
}

__setup("logbuf.console", logbuf_console_setup);
#endif

MODULE_DESCRIPTION("semidrive log buffer driver");
MODULE_AUTHOR("Xingyu Chen <xingyu.chen@semidrive.com>");
MODULE_LICENSE("GPL v2");
