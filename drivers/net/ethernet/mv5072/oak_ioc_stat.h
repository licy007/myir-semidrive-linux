/* ***************************************************
 * Model File: OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_ioc_stat
 * Model Path: OAK --- DBType=0;Connect=Provider=MSDASQL.1;Persist Security Info=False;Data Source=OAK;LazyLoad=1;
 *
 * 2019-05-16  - 11:40
 * ***************************************************
 *  */
#ifndef H_OAK_IOC_STAT
#define H_OAK_IOC_STAT

typedef struct oak_ioc_statStruct
{
#define OAK_IOCTL_STAT (SIOCDEVPRIVATE + 4)
#define OAK_IOCTL_STAT_GET_TXS (1 << 0)
#define OAK_IOCTL_STAT_GET_RXS (1 << 1)
#define OAK_IOCTL_STAT_GET_TXC (1 << 2)
#define OAK_IOCTL_STAT_GET_RXC (1 << 3)
#define OAK_IOCTL_STAT_GET_RXB (1 << 4)
#define OAK_IOCTL_STAT_GET_LDG (1 << 5)
    __u32 cmd;
    __u32 idx;
    __u32 offs;
    char data[128];
    int error;
} oak_ioc_stat;

#endif /* #ifndef H_OAK_IOC_STAT */

