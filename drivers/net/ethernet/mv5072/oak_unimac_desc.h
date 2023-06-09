/* ***************************************************
 * Model File: OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_unimac_desc
 * Model Path: OAK --- DBType=0;Connect=Provider=MSDASQL.1;Persist Security Info=False;Data Source=OAK;LazyLoad=1;
 *
 * 2019-05-16  - 11:40
 * ***************************************************
 *  */
#ifndef H_OAK_UNIMAC_DESC
#define H_OAK_UNIMAC_DESC

typedef struct oak_rxd_tStruct
{
    uint32_t buf_ptr_lo;
    uint32_t buf_ptr_hi;
} oak_rxd_t;

typedef struct oak_rxs_tStruct
{
    uint32_t bc : 16;
    uint32_t es : 1;
    uint32_t ec : 2;
    uint32_t res1 : 1;
    uint32_t first_last : 2;
    uint32_t ipv4_hdr_ok : 1;
    uint32_t l4_chk_ok : 1;
    uint32_t l4_prot : 2;
    uint32_t res2 : 1;
    uint32_t l3_ipv4 : 1;
    uint32_t l3_ipv6 : 1;
    uint32_t vlan : 1;
    uint32_t l2_prot : 2;
    uint32_t timestamp : 32;
    uint32_t rc_chksum : 16;
    uint32_t udp_cs_0 : 1;
    uint32_t res3 : 15;
    uint32_t mhdr : 16;
    uint32_t mhok : 1;
    uint32_t res4 : 15;
} oak_rxs_t;

typedef struct oak_txd_tStruct
{
    uint32_t bc : 16;
    uint32_t res1 : 4;
    uint32_t last : 1;
    uint32_t first : 1;
    uint32_t gl3_chksum : 1;
    uint32_t gl4_chksum : 1;
    uint32_t res2 : 4;
    uint32_t time_valid : 1;
    uint32_t res3 : 3;
    uint32_t timestamp : 32;
    uint32_t buf_ptr_lo : 32;
    uint32_t buf_ptr_hi : 32;
} oak_txd_t;

#endif /* #ifndef H_OAK_UNIMAC_DESC */

