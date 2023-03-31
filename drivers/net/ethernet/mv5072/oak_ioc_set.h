/* ***************************************************
 * Model File: OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_ioc_set
 * Model Path: OAK --- DBType=0;Connect=Provider=MSDASQL.1;Persist Security Info=False;Data Source=OAK;LazyLoad=1;
 *
 * 2019-05-16  - 11:40
 * ***************************************************
 *  */
#ifndef H_OAK_IOC_SET
#define H_OAK_IOC_SET

typedef struct oak_ioc_setStruct
{
#define OAK_IOCTL_SET_MAC_RATE_A (SIOCDEVPRIVATE + 5)
#define OAK_IOCTL_SET_MAC_RATE_B (SIOCDEVPRIVATE + 6)
#define OAK_IOCTL_SET_TXR_RATE (SIOCDEVPRIVATE + 7)
    __u32 cmd;
    __u32 idx;
    __u32 data;
    int error;
} oak_ioc_set;

#endif /* #ifndef H_OAK_IOC_SET */

