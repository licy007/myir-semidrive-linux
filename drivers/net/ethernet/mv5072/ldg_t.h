/* ***************************************************
 * Model File: OAK-Linux::OAK Switch Board - Linux Driver::Class Model::ldg_t
 * Model Path: OAK --- DBType=0;Connect=Provider=MSDASQL.1;Persist Security Info=False;Data Source=OAK;LazyLoad=1;
 *
 * 2019-05-16  - 11:40
 * ***************************************************
 *  */
#ifndef H_LDG_T
#define H_LDG_T

typedef struct ldg_tStruct
{
    struct napi_struct napi;
    struct oak_tStruct* device;
    uint64_t msi_tx;
    uint64_t msi_rx;
    uint64_t msi_te;
    uint64_t msi_re;
    uint64_t msi_ge;
    uint64_t irq_mask;
    uint32_t irq_first;
    uint32_t irq_count;
    uint32_t msi_grp;
    char msiname[32];
} ldg_t;

#endif /* #ifndef H_LDG_T */

