/* ***************************************************
 * Model File: OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_ioc_flow
 * Model Path: OAK --- DBType=0;Connect=Provider=MSDASQL.1;Persist Security Info=False;Data Source=OAK;LazyLoad=1;
 *
 * 2019-05-16  - 11:40
 * ***************************************************
 *  */
#ifndef H_OAK_IOC_FLOW
#define H_OAK_IOC_FLOW

typedef struct oak_ioc_flowStruct
{
#define OAK_IOCTL_RXFLOW (SIOCDEVPRIVATE + 8)
#define OAK_IOCTL_RXFLOW_CLEAR 0
#define OAK_IOCTL_RXFLOW_MGMT (1 << 0)
#define OAK_IOCTL_RXFLOW_QPRI (1 << 3)
#define OAK_IOCTL_RXFLOW_SPID (1 << 7)
#define OAK_IOCTL_RXFLOW_FLOW (1 << 12)
#define OAK_IOCTL_RXFLOW_DA (1 << 18)
#define OAK_IOCTL_RXFLOW_ET (1 << 19)
#define OAK_IOCTL_RXFLOW_FID (1 << 20)
#define OAK_IOCTL_RXFLOW_DA_MASK (1 << 21)
    __u32 cmd;
    __u32 idx;
    uint32_t val_lo;
    uint32_t val_hi;
    char data[16];
    int ena;
    int error;
} oak_ioc_flow;

#endif /* #ifndef H_OAK_IOC_FLOW */

