/* used in kernel for cam isr function call
 * in isr,put log to buffer when need printf, then wakeup thread
 * thread call printk to show log, add as below
 * 1) one thread for call printk, and this thread can do more for debug
 * 2) ring buffer for log.
 */

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>
#include "cam_isr_log.h"

#define SIZE_LOG_BUF (1024)

DECLARE_COMPLETION(log_completion);
static struct task_struct *thrd_handle;
//-------------------------------------
//ring buffer
struct ring_buffer_tag {
	char buf[SIZE_LOG_BUF];
	int in, out;
	int cnt;
	spinlock_t lock;
};
static struct ring_buffer_tag rbuf;
// drop log when not init
static atomic_t at_inited = ATOMIC_INIT(0);
static atomic_t at_heartbeat = ATOMIC_INIT(0);
//-------------------------------------heartbeat

#ifdef CAMERA_HEARTBEAT
void camera_heartbeat_clear(void)
{
	atomic_set(&at_heartbeat, 0);
	return;
}
void camera_heartbeat_inc(void)
{
	atomic_inc(&at_heartbeat);
	return;
}
#else
void camera_heartbeat_clear(void)
{
	return;
}
void camera_heartbeat_inc(void)
{
	return;
}
#endif //CAMERA_HEARTBEAT
//-------------------------------------rbuf
static void rbuf_init(void)
{
	spin_lock_init(&rbuf.lock);
	rbuf.cnt = 0;
	rbuf.in = 0;
	rbuf.out = 0;
}

// input: p:string, len:not contain '\0'
// p != NULL, len > 0
// return:remain
static int rbuf_put(char *p, int len)
{
	unsigned long flags;
	register int i;

	spin_lock_irqsave(&rbuf.lock, flags);
	// buf not enough,drop debug,notice
	if (rbuf.cnt >= (SIZE_LOG_BUF * 4 / 5)) {
		if (rbuf.cnt >= (SIZE_LOG_BUF - 1) || *p == 0xA7 || *p == 0xA6) {
			spin_unlock_irqrestore(&rbuf.lock, flags);
			return len;
		}
	}
	i = ((len + 1) <= (SIZE_LOG_BUF - rbuf.cnt)) ? len : (SIZE_LOG_BUF - rbuf.cnt) - 1;
	len -= i;
	rbuf.cnt += (i + 1);
	while (i--) {
		rbuf.buf[rbuf.in++] = *p++;
		rbuf.in %= SIZE_LOG_BUF;
	}
	// last '\0'
	rbuf.buf[rbuf.in++] = '\0';
	rbuf.in %= SIZE_LOG_BUF;
	spin_unlock_irqrestore(&rbuf.lock, flags);

	return len;
}

// input: p != NULL, max > 0
// return: len of string(contain \0)
static int rbuf_get(char *p, int max)
{
	unsigned long flags;
	register int i;

	spin_lock_irqsave(&rbuf.lock, flags);

	if (rbuf.cnt < 1) {
		spin_unlock_irqrestore(&rbuf.lock, flags);
		return 0;
	}
	// how many char should get
	max = (rbuf.cnt <= max) ? rbuf.cnt : max - 1;
	i = 0;
	while(i < max) {
		*p = rbuf.buf[rbuf.out];
		rbuf.out = (rbuf.out + 1) % SIZE_LOG_BUF;
		i++;
		if (*p == '\0') {
			rbuf.cnt -= i;
			spin_unlock_irqrestore(&rbuf.lock, flags);
			return i;
		}
		p++;
	}
	p--;
	*p = '\0';
	rbuf.cnt -= i;
	spin_unlock_irqrestore(&rbuf.lock, flags);

	return i;
}

int cam_isr_log(int level, const char *a, ...)
{
	va_list ap;
	char str[160];
	int i;
	u64 t;

	if (level > console_loglevel) { // not show set by printk
		return 0;
	}
	if (atomic_read(&at_inited) == 0){
		return 0;
	}
	str[0] = (char)(0xA0 | (level & 0xF));
	i = 1;
	t = local_clock() / NSEC_PER_USEC; // the same as printk
	i += snprintf(str + 1, sizeof(str) - 1, "[%llu.%llu]",
					t / USEC_PER_SEC, t % USEC_PER_SEC);

	va_start(ap, a);
	i += vsnprintf(str + i, sizeof(str) - i, a, ap);
	va_end(ap);

	i = rbuf_put(str, i);
	complete(&log_completion);

	return i;
}

EXPORT_SYMBOL(cam_isr_log);

int cam_isr_log_thread(void *pdat)
{
	char str[160];
	uint32_t last_heartbeat = (uint32_t)0;
	u64 time_distance = 0ull;

	do {
		if (rbuf_get(str, sizeof(str))) {
			switch ((unsigned char)str[0]) {
			case 0xA0:
				pr_emerg("%s", &str[1]);
				break;
			case 0xA1:
				pr_alert("%s", &str[1]);
				break;
			case 0xA2:
				pr_crit("%s", &str[1]);
				break;
			case 0xA3:
				pr_err("%s", &str[1]);
				break;
			case 0xA4:
				pr_warn("%s", &str[1]);
				break;
			case 0xA5:
				pr_info("%s", &str[1]);
				break;
			case 0xA6:
				pr_notice("%s", &str[1]);
				break;
			case 0xA7:
				// pr_debug == no_printk when user version, so used pr_info
				pr_info("%s", &str[1]);
				break;
			default:
				pr_info("%s", str);
			}
		} else {
			u64 tmp;

			tmp = local_clock();
			// show frame number per second
			if (((uint32_t)atomic_read(&at_heartbeat) != last_heartbeat) &&
				(tmp - time_distance) / NSEC_PER_MSEC >= 1000) {
				time_distance = tmp;
				last_heartbeat = (uint32_t)atomic_read(&at_heartbeat);
				pr_err("cam heartbeat:%u\n", last_heartbeat);
			}
			tmp = 1000 - ((tmp - time_distance) / NSEC_PER_MSEC);
			tmp %= 1000;
			//wait_for_completion_timeout(&log_completion, msecs_to_jiffies(tmp));
			wait_for_completion_interruptible_timeout(&log_completion, msecs_to_jiffies(tmp));
		}
	} while (!kthread_should_stop());
	thrd_handle = NULL;

	return 0;
}

static int __init cam_isr_log_init(void)
{
	rbuf_init();
	thrd_handle =
	    kthread_run(cam_isr_log_thread, NULL, "%s", "cam_isr_log");
	if (thrd_handle == NULL)
		pr_err("create thread for cam_isr_log fail\n");

	pr_info("cam_isr_log_init done\n");
	atomic_set(&at_inited, 1);

	return 0;
}

static void __exit cam_isr_log_exit(void)
{
	if (thrd_handle) {
		kthread_stop(thrd_handle);
		thrd_handle = NULL;
	}

}
// Earlier than module_init for used by cam drv
fs_initcall(cam_isr_log_init);
//module_init(cam_isr_log_init);
module_exit(cam_isr_log_exit);
