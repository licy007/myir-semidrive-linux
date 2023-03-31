
// return: how many chars can't put to buffer;
int cam_isr_log(int level, const char *a, ...);

// log level.20220705:printk default:4
#define cam_isr_log_emerg(a, ...)   cam_isr_log(0x0, a, ##__VA_ARGS__)
#define cam_isr_log_alert(a, ...)   cam_isr_log(0x1, a, ##__VA_ARGS__)
#define cam_isr_log_crit(a, ...)    cam_isr_log(0x2, a, ##__VA_ARGS__)
#define cam_isr_log_err(a, ...)     cam_isr_log(0x3, a, ##__VA_ARGS__)
#define cam_isr_log_warn(a, ...)    cam_isr_log(0x4, a, ##__VA_ARGS__)
#define cam_isr_log_warning(a, ...)  cam_isr_log(0x4, a, ##__VA_ARGS__)
#define cam_isr_log_info(a, ...)    cam_isr_log(0x5, a, ##__VA_ARGS__)
#define cam_isr_log_notice(a, ...)  cam_isr_log(0x6, a, ##__VA_ARGS__)
#define cam_isr_log_debug(a, ...)   cam_isr_log(0x7, a, ##__VA_ARGS__)
#define cam_isr_log_trace(a, ...)   cam_isr_log(0x9, a, ##__VA_ARGS__)


// for camera heartbeat:define ~ enable,not define ~ disable
#define CAMERA_HEARTBEAT

void camera_heartbeat_clear(void);
void camera_heartbeat_inc(void);
