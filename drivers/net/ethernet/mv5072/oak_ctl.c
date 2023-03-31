/*
 *        LICENSE:
 *        (C)Copyright 2010-2011 Marvell.
 *
 *        All Rights Reserved
 *
 *        THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF MARVELL
 *        The copyright notice above does not evidence any
 *        actual or intended publication of such source code.
 *
 *        This Module contains Proprietary Information of Marvell
 *        and should be treated as Confidential.
 *
 *        The information in this file is provided for the exclusive use of
 *        the licensees of Marvell. Such users have the right to use, modify,
 *        and incorporate this code into products for purposes authorized by
 *        the license agreement provided they include this notice and the
 *        associated copyright notice with any such product.
 *
 *        The information in this file is provided "AS IS" without warranty.
 *
 * Contents: collection of functions that provide user IO-control requests
 *
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak_ctl
 * Author: afischer
 * Date  :2020-05-11  - 10:11
 *
 *  */
#include "oak_ctl.h"

extern int debug;

/* private function prototypes */
static uint32_t oak_ctl_esu_rd32(oak_t* np, oak_ioc_reg* req);
static void oak_ctl_esu_wr32(oak_t* np, oak_ioc_reg* req);
static int oak_ctl_lgen_init(oak_t* np, oak_ioc_lgen* req);
static int oak_ctl_lgen_release(oak_t* np, oak_ioc_lgen* req);
static int oak_ctl_lgen_rx_data(oak_t* np, oak_ioc_lgen* req);
static int oak_ctl_lgen_rx_stop(oak_t* np, oak_ioc_lgen* req);
static int oak_ctl_lgen_rx_start(oak_t* np, oak_ioc_lgen* req);
static int oak_ctl_lgen_tx_start(oak_t* np, oak_ioc_lgen* req);
static int oak_ctl_lgen_tx_stop(oak_t* np, oak_ioc_lgen* req);
static int oak_ctl_lgen_tx_data(oak_t* np, oak_ioc_lgen* req);
static int oak_ctl_lgen_txrx_reset(oak_t* np, oak_ioc_lgen* req);

/* Name      : channel_status_access
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_channel_status_access(oak_t* np, struct ifreq* ifr, int cmd)
{
    oak_ioc_stat req;
    int rc; /* automatically added for object flow handling */
    int rc_1; /* start of activity code */
    /* UserCode{F34AA6EC-27B5-452a-BEE5-FA043690008D}:l6ARB3E6qd */
    rc = copy_from_user(&req, ifr->ifr_data, sizeof(req));
    /* UserCode{F34AA6EC-27B5-452a-BEE5-FA043690008D} */

    if (rc == 0)
    {

        if (req.cmd == OAK_IOCTL_STAT_GET_TXS)
        {

            if (req.idx < np->num_tx_chan)
            {
                /* UserCode{35DF9CCE-A639-4d25-9630-2CF684350DF3}:SQ2S65WXZj */
                oak_chan_info* tar = (oak_chan_info*)req.data;
                oak_tx_chan_t* src = &np->tx_channel[req.idx];
                tar->flags = src->flags;
                tar->r_size = src->tbr_size; /* RX/TX ring size */
#if 0
tar->r_pend = src->tbr_pend ; /* number of pending ring buffers */
#else
                tar->r_pend = atomic_read(&src->tbr_pend);
#endif
                tar->r_widx = src->tbr_widx; /* write buffer index */
                tar->r_ridx = src->tbr_ridx; /* read buffer index */

                /* UserCode{35DF9CCE-A639-4d25-9630-2CF684350DF3} */
            }
            else
            {
                /* UserCode{9854F59E-6023-42e0-8743-8957960F4DCF}:17zylXIV9A */
                rc = -ENOMEM;
                /* UserCode{9854F59E-6023-42e0-8743-8957960F4DCF} */
            }
        }
        else
        {

            if (req.cmd == OAK_IOCTL_STAT_GET_RXS)
            {

                if (req.idx < np->num_rx_chan)
                {
                    /* UserCode{A067A39E-44A4-40bb-834D-B3DDF25E7120}:bxwlZFt4bc */
                    oak_chan_info* tar = (oak_chan_info*)req.data;
                    oak_rx_chan_t* src = &np->rx_channel[req.idx];
                    tar->flags = src->flags;
                    tar->r_size = src->rbr_size; /* RX/TX ring size */
#if 0
tar->r_pend = src->rbr_pend ; /* number of pending ring buffers */
#else
                    tar->r_pend = atomic_read(&src->rbr_pend);
#endif
                    tar->r_widx = src->rbr_widx; /* write buffer index */
                    tar->r_ridx = src->rbr_ridx; /* read buffer index */
                    /* UserCode{A067A39E-44A4-40bb-834D-B3DDF25E7120} */
                }
                else
                {
                    /* UserCode{A7DC593F-DABE-422f-92AD-5E39950B207E}:17zylXIV9A */
                    rc = -ENOMEM;
                    /* UserCode{A7DC593F-DABE-422f-92AD-5E39950B207E} */
                }
            }
            else
            {

                if (req.cmd == OAK_IOCTL_STAT_GET_TXC)
                {

                    if ((req.idx < np->num_tx_chan) && (req.offs < np->tx_channel[req.idx].tbr_size))
                    {
                        /* UserCode{2C832C29-F277-4133-824E-6E8B8063FC7E}:3CHMZzeJvW */
                        memcpy(req.data, &np->tx_channel[req.idx].tbr[req.offs], sizeof(oak_txd_t));
                        /* UserCode{2C832C29-F277-4133-824E-6E8B8063FC7E} */
                    }
                    else
                    {
                        /* UserCode{CD29AD8F-2424-4e9e-973D-023A04391A44}:17zylXIV9A */
                        rc = -ENOMEM;
                        /* UserCode{CD29AD8F-2424-4e9e-973D-023A04391A44} */
                    }
                }
                else
                {

                    if (req.cmd == OAK_IOCTL_STAT_GET_RXC)
                    {

                        if ((req.idx < np->num_rx_chan) && (req.offs < np->rx_channel[req.idx].rbr_size))
                        {
                            /* UserCode{7BF86B7A-DFAF-44b4-8586-6E503F672048}:1JX09XBH5A */
                            memcpy(req.data, &np->rx_channel[req.idx].rsr[req.offs], sizeof(oak_rxs_t));

                            /* UserCode{7BF86B7A-DFAF-44b4-8586-6E503F672048} */
                        }
                        else
                        {
                            /* UserCode{21549931-9E1E-4569-A756-1D2245006629}:17zylXIV9A */
                            rc = -ENOMEM;
                            /* UserCode{21549931-9E1E-4569-A756-1D2245006629} */
                        }
                    }
                    else
                    {

                        if (req.cmd == OAK_IOCTL_STAT_GET_RXB)
                        {

                            if ((req.idx < np->num_rx_chan) && (req.offs < np->rx_channel[req.idx].rbr_size))
                            {
                                /* UserCode{4EE8FD73-DE16-44d1-9CF6-063AB836097D}:1UWCCUh5z6 */
                                memcpy(req.data, &np->rx_channel[req.idx].rbr[req.offs], sizeof(oak_rxd_t));
                                /* UserCode{4EE8FD73-DE16-44d1-9CF6-063AB836097D} */
                            }
                            else
                            {
                                /* UserCode{492C3BEE-6AF5-48b7-BC93-75D7C7562CB7}:17zylXIV9A */
                                rc = -ENOMEM;
                                /* UserCode{492C3BEE-6AF5-48b7-BC93-75D7C7562CB7} */
                            }
                        }
                        else
                        {

                            if (req.cmd == OAK_IOCTL_STAT_GET_LDG)
                            {

                                if ((req.idx < np->gicu.num_ldg) && (sizeof(req.data) >= (8 * sizeof(uint64_t))))
                                {
                                    /* UserCode{28E9CA47-5BCD-47af-AB66-E88D7DF1D33F}:MK1slGJH7a */
                                    uint64_t* tar = (uint64_t*)req.data;
                                    tar[0] = np->gicu.num_ldg;
                                    if (np->num_rx_chan < np->num_tx_chan)
                                    {
                                        tar[1] = np->num_tx_chan;
                                    }
                                    else
                                    {
                                        tar[1] = np->num_rx_chan;
                                    }
                                    tar[2] = np->gicu.ldg[req.idx].msi_tx;
                                    tar[3] = np->gicu.ldg[req.idx].msi_te;
                                    tar[4] = np->gicu.ldg[req.idx].msi_rx;
                                    tar[5] = np->gicu.ldg[req.idx].msi_re;
                                    tar[6] = np->gicu.ldg[req.idx].msi_ge;
                                    tar[7] = np->gicu.msi_vec[req.idx].vector;
                                    /* UserCode{28E9CA47-5BCD-47af-AB66-E88D7DF1D33F} */
                                }
                                else
                                {
                                    /* UserCode{5B10FB38-0325-4d6c-BD08-9E6B43C1A58E}:17zylXIV9A */
                                    rc = -ENOMEM;
                                    /* UserCode{5B10FB38-0325-4d6c-BD08-9E6B43C1A58E} */
                                }
                            }
                            else
                            {
                                /* UserCode{7FE403F6-CEC8-4240-8FAD-C5CA0D176702}:FubMmJTDKN */
                                rc = -EINVAL;
                                /* UserCode{7FE403F6-CEC8-4240-8FAD-C5CA0D176702} */
                            }
                        }
                    }
                }
            }
        }
        /* UserCode{5B3C2789-2206-4e48-AB74-7298EC30177D}:UkatlF1Wx3 */
        req.error = rc;
        rc = copy_to_user(ifr->ifr_data, &req, sizeof(req));
        /* UserCode{5B3C2789-2206-4e48-AB74-7298EC30177D} */
    }
    else
    {
    }
    rc_1 = rc;
    /* UserCode{81560891-39B6-4d6c-9F43-52D70FF6F3FD}:T7ELqq2Eec */
    oakdbg(debug, DRV, "np-level=%d cmd=0x%x req=0x%x rc=%d", np->level, cmd, req.cmd, rc);
    /* UserCode{81560891-39B6-4d6c-9F43-52D70FF6F3FD} */

    return rc_1;
}

/* Name      : esu_rd32
 * Returns   : uint32_t
 * Parameters:  oak_t * np = np,  oak_ioc_reg * req = req
 *  */
static uint32_t oak_ctl_esu_rd32(oak_t* np, oak_ioc_reg* req)
{
    /* automatically added for object flow handling */
    uint32_t rc_1 = 0; /* start of activity code */
    /* UserCode{FECF5077-0413-48ff-BB0E-66C26AA161CC}:1LCij4OtQQ */
    uint32_t offs = req->offs;
    uint32_t reg = (offs & 0x1F);
    uint32_t val;

    offs &= ~0x0000001f;
    offs |= (req->dev_no << 7) | (reg << 2);

    /* UserCode{FECF5077-0413-48ff-BB0E-66C26AA161CC} */

    /* UserCode{FDCB5F73-5538-4f83-9E49-A0F50CD90EBF}:r0Rynu0uqE */
    val = sr32(np, offs);
    /* UserCode{FDCB5F73-5538-4f83-9E49-A0F50CD90EBF} */

    /* UserCode{7265991E-427A-4c00-B555-C8687444FA28}:119qb1dGuY */
    oakdbg(debug, DRV, "ESU RD at offset: 0x%x device: %d data=0x%x", offs, req->dev_no, val);
    /* UserCode{7265991E-427A-4c00-B555-C8687444FA28} */

    rc_1 = val;
    return rc_1;
}

/* Name      : esu_wr32
 * Returns   : void
 * Parameters:  oak_t * np = np,  oak_ioc_reg * req = req
 *  */
static void oak_ctl_esu_wr32(oak_t* np, oak_ioc_reg* req)
{
    /* start of activity code */
    /* UserCode{585F4434-507C-4c05-AE8E-E93578B0B1BA}:1YqGmMjpuN */
    uint32_t offs = req->offs;
    uint32_t reg = (offs & 0x1F);

    offs &= ~0x0000001f;
    offs |= (req->dev_no << 7) | (reg << 2);
    /* UserCode{585F4434-507C-4c05-AE8E-E93578B0B1BA} */

    /* UserCode{FC239E06-9ADA-4828-89EB-2E78D6350240}:1efotd5Dsz */
    sw32(np, offs, req->data);
    /* UserCode{FC239E06-9ADA-4828-89EB-2E78D6350240} */

    /* UserCode{214A7204-3051-4abb-9A30-A8F2B21C0B38}:bHeFvC2vGL */
    oakdbg(debug, DRV, "ESU WR at offset: 0x%x device: %d data=0x%x", offs, req->dev_no, req->data);
    /* UserCode{214A7204-3051-4abb-9A30-A8F2B21C0B38} */

    return;
}

/* Name      : lgen_rx_pkt
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  int desc_num = desc_num,  struct sk_buff ** target = target
 *  */
int oak_ctl_lgen_rx_pkt(oak_t* np, uint32_t ring, int desc_num, struct sk_buff** target)
{
    oak_rx_chan_t* rxc = &np->rx_channel[ring]; /* automatically added for object flow handling */
    int rc_4 = desc_num; /* start of activity code */
    /* UserCode{8DB37BC2-42C6-406e-AAF5-57E63FA52A4E}:2b6AJ3yA0o */
    *target = (struct sk_buff*)NULL;
    /* UserCode{8DB37BC2-42C6-406e-AAF5-57E63FA52A4E} */

    if (rxc->rbr_count > 0 && (desc_num <= rxc->rbr_count))
    {
        /* UserCode{C1355A53-78A1-401b-9618-FD331383FC98}:1Fz0QHJC3F */
        rxc->rbr_count -= desc_num;
        rxc->rbr_ridx = (rxc->rbr_ridx + desc_num) % rxc->rbr_size;
        rxc->rbr_widx = (rxc->rbr_widx + desc_num) % rxc->rbr_size;

        /* UserCode{C1355A53-78A1-401b-9618-FD331383FC98} */

        /* UserCode{70170DEA-3A1C-4e74-85C5-5CD26742C0BE}:4FWMn4S9TN */
        wmb();
        /* UserCode{70170DEA-3A1C-4e74-85C5-5CD26742C0BE} */

        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_CPU_PTR(ring), rxc->rbr_widx & 0x7ff);
    }
    else
    {
    }
    return rc_4;
}

/* Name      : lgen_xmit_frame
 * Returns   : void
 * Parameters:  oak_t * np = np,  oak_tx_chan_t * txc = txc,  uint32_t idx = idx,  uint32_t num = num
 *  */
void oak_ctl_lgen_xmit_frame(oak_t* np, oak_tx_chan_t* txc, uint32_t idx, uint32_t num)
{
    uint32_t tbr_pend; /* start of activity code */
    /* UserCode{013993CB-7C8F-4cb6-8126-65BAEA13C9C0}:1BPpB7UjZ4 */
    tbr_pend = atomic_read(&txc->tbr_pend);

    /* UserCode{013993CB-7C8F-4cb6-8126-65BAEA13C9C0} */

    if ((txc->flags & OAK_RX_LGEN_TX_MODE) == 0)
    {
        /* UserCode{AF0C83AC-928B-4015-BC47-F087172210CE}:1OuhEysKrV */
        txc->tbr_widx = txc->tbr_ridx;
        txc->flags = OAK_RX_LGEN_TX_MODE;

        /* UserCode{AF0C83AC-928B-4015-BC47-F087172210CE} */
    }
    else
    {
    }

    if (tbr_pend < txc->tbr_size)
    {

        if (txc->tbr_widx + num > tbr_pend)
        {
            /* UserCode{AAE3FE4D-0493-4fe6-ADAD-50CEA11D3100}:auxuwROPFf */
            txc->tbr_widx = tbr_pend;
            /* UserCode{AAE3FE4D-0493-4fe6-ADAD-50CEA11D3100} */
        }
        else
        {
            /* UserCode{DE1BA64B-C654-445e-A653-44E17DE9B190}:onbZysvDMw */
            txc->tbr_widx += num;
            /* UserCode{DE1BA64B-C654-445e-A653-44E17DE9B190} */
        }
    }
    else
    {

        if (num >= txc->tbr_size)
        {
            /* UserCode{EAD0D98A-8F2C-4bed-954A-73D906A719C4}:DRkM9C2p84 */
            txc->tbr_widx = txc->tbr_widx + txc->tbr_size - 1;
            /* UserCode{EAD0D98A-8F2C-4bed-954A-73D906A719C4} */
        }
        else
        {
            /* UserCode{D0C22223-4DF3-447c-9000-B6A285154833}:onbZysvDMw */
            txc->tbr_widx += num;
            /* UserCode{D0C22223-4DF3-447c-9000-B6A285154833} */
        }
    }
    /* UserCode{8FA4C833-61C0-4e67-BFCC-1E9137B4BCB1}:slJKibBX7c */
    txc->tbr_widx %= txc->tbr_size;
    /* UserCode{8FA4C833-61C0-4e67-BFCC-1E9137B4BCB1} */

    /* UserCode{37CF7DE4-B695-421a-A848-C424C91F8A50}:4FWMn4S9TN */
    wmb();
    /* UserCode{37CF7DE4-B695-421a-A848-C424C91F8A50} */

    oak_unimac_io_write_32(np, OAK_UNI_TX_RING_CPU_PTR(idx), txc->tbr_widx & 0x7ff);
    return;
}

/* Name      : process_tx_pkt_lgen
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  int desc_num = desc_num
 *  */
int oak_ctl_process_tx_pkt_lgen(oak_t* np, uint32_t ring, int desc_num)
{
    uint32_t done;
    oak_tx_chan_t* txc = &np->tx_channel[ring];
    uint32_t cnt; /* automatically added for object flow handling */
    int rc_5; /* start of activity code */
    /* UserCode{F8BDAD29-0CDC-43d4-8925-CAC4AC77A2BC}:1ZKQnEKO9z */
    done = desc_num & (txc->tbr_size - 1);
    for (cnt = 0; cnt < done; cnt++)
    {
        txc->stat.tx_byte_count += txc->tbr[txc->tbr_ridx + cnt].bc;
    }
    txc->tbr_ridx = (txc->tbr_ridx + done) % txc->tbr_size;
    txc->stat.tx_frame_count += done;

    /* UserCode{F8BDAD29-0CDC-43d4-8925-CAC4AC77A2BC} */

    if (txc->tbr_count > 0)
    {

        if (txc->tbr_count > txc->tbr_size - 1)
        {
            /* UserCode{4AC392EA-A6FA-4372-94B8-50B35CE56471}:13WceYb5qT */
            txc->tbr_compl += done;
            /* UserCode{4AC392EA-A6FA-4372-94B8-50B35CE56471} */

            if (txc->tbr_compl < txc->tbr_size - 1)
            {
                /* UserCode{AF76E127-53C5-4a9d-92C5-49CB7FE404FB}:mbWDcSBIbU */
                done = 0;
                /* UserCode{AF76E127-53C5-4a9d-92C5-49CB7FE404FB} */
            }
            else
            {
                /* UserCode{808872C6-EEB8-45d4-9745-AAD617ACD73A}:1HfhfZ0alN */
                txc->tbr_count -= txc->tbr_compl;
                /* UserCode{808872C6-EEB8-45d4-9745-AAD617ACD73A} */

                if (txc->tbr_count < txc->tbr_compl)
                {
                    /* UserCode{361E675C-C63C-4971-B448-BA4DE6FDCAC0}:1CvqnRCgT4 */
                    done = txc->tbr_count; /* do the rest */
                    /* UserCode{361E675C-C63C-4971-B448-BA4DE6FDCAC0} */
                }
                else
                {
                    /* UserCode{C7A15C41-92B0-4245-B925-551FEE1EF94D}:Hsz4akZJuU */
                    done = txc->tbr_compl; /* do as much as possible */
                    /* UserCode{C7A15C41-92B0-4245-B925-551FEE1EF94D} */
                }
            }
        }
        else
        {
            /* UserCode{BC64E682-6389-4d0c-9558-02993CE757AB}:ECRGXU7yth */
            txc->tbr_count -= done;
            done = 0;

            /* UserCode{BC64E682-6389-4d0c-9558-02993CE757AB} */
        }

        if (done > 0)
        {
            /* UserCode{786227B0-A637-4978-A873-DC2EE2464AC2}:12YGpzUyhP */
            txc->tbr_compl = 0;
            /* UserCode{786227B0-A637-4978-A873-DC2EE2464AC2} */

            oak_ctl_lgen_xmit_frame(np, txc, ring, done);
        }
        else
        {
        }
    }
    else
    {
    }
    rc_5 = desc_num;
    return rc_5;
}

/* Name      : set_mac_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd
 *  */
int oak_ctl_set_mac_rate(oak_t* np, struct ifreq* ifr, int cmd)
{
    uint32_t cls = OAK_MIN_TX_RATE_CLASS_A;
    oak_ioc_set ioc;
    int rc; /* automatically added for object flow handling */
    int rc_12; /* start of activity code */
    /* UserCode{7B29C9BF-6582-4c9d-99FB-4432B4BC6C25}:lYS0x0eI4j */
    rc = copy_from_user(&ioc, ifr->ifr_data, sizeof(ioc));
    /* UserCode{7B29C9BF-6582-4c9d-99FB-4432B4BC6C25} */

    if (rc == 0)
    {

        if (cmd == OAK_IOCTL_SET_MAC_RATE_B)
        {
            /* UserCode{E6381D62-9848-4aef-BFF6-433921A21704}:4FUg3z82bZ */
            cls = OAK_MIN_TX_RATE_CLASS_B;
            /* UserCode{E6381D62-9848-4aef-BFF6-433921A21704} */
        }
        else
        {
        }

        if (ioc.idx > 0)
        {
            rc = oak_unimac_set_tx_ring_rate(np, ioc.idx - 1, cls, 0x600, ioc.data & 0x1FFFF);
        }
        else
        {
            rc = oak_unimac_set_tx_mac_rate(np, cls, 0x600, ioc.data & 0x1FFFF);
        }
    }
    else
    {
    }
    rc_12 = rc;
    return rc_12;
}

/* Name      : set_rx_flow
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_set_rx_flow(oak_t* np, struct ifreq* ifr, int cmd)
{
    oak_ioc_flow ioc;
    int rc;
    uint16_t v0;
    uint16_t v1; /* automatically added for object flow handling */
    int rc_37; /* start of activity code */
    /* UserCode{A8508237-147F-4653-8B01-BA2D85A883C4}:lYS0x0eI4j */
    rc = copy_from_user(&ioc, ifr->ifr_data, sizeof(ioc));
    /* UserCode{A8508237-147F-4653-8B01-BA2D85A883C4} */

    if (rc == 0)
    {
        /* UserCode{FD4D50DA-9FEE-42c5-9416-A292928822F3}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{FD4D50DA-9FEE-42c5-9416-A292928822F3} */

        if (ioc.idx > 0 && ioc.idx < 10)
        {

            if (ioc.cmd == OAK_IOCTL_RXFLOW_CLEAR)
            {
                oak_unimac_set_rx_none(np, ioc.idx);
                /* UserCode{F6384BE8-8793-4c4b-8B3C-20B2EE7A16F3}:r8srRGlzXT */
                rc = 0;
                /* UserCode{F6384BE8-8793-4c4b-8B3C-20B2EE7A16F3} */
            }
            else
            {

                if (ioc.cmd == OAK_IOCTL_RXFLOW_MGMT)
                {
                    oak_unimac_set_rx_mgmt(np, ioc.idx, ioc.val_lo, ioc.ena);
                    /* UserCode{75B2709C-68A1-418d-B8B7-2859F3EC50AB}:r8srRGlzXT */
                    rc = 0;
                    /* UserCode{75B2709C-68A1-418d-B8B7-2859F3EC50AB} */
                }
                else
                {

                    if (ioc.cmd == OAK_IOCTL_RXFLOW_QPRI)
                    {
                        oak_unimac_set_rx_8021Q_qpri(np, ioc.idx, ioc.val_lo, ioc.ena);
                        /* UserCode{48370F49-1221-456c-A40A-C62A18330668}:r8srRGlzXT */
                        rc = 0;
                        /* UserCode{48370F49-1221-456c-A40A-C62A18330668} */
                    }
                    else
                    {

                        if (ioc.cmd == OAK_IOCTL_RXFLOW_SPID)
                        {
                            oak_unimac_set_rx_8021Q_spid(np, ioc.idx, ioc.val_lo, ioc.ena);
                            /* UserCode{42BDE506-C0D4-4ab2-9C62-361F53779E25}:r8srRGlzXT */
                            rc = 0;
                            /* UserCode{42BDE506-C0D4-4ab2-9C62-361F53779E25} */
                        }
                        else
                        {

                            if (ioc.cmd == OAK_IOCTL_RXFLOW_FLOW)
                            {
                                oak_unimac_set_rx_8021Q_flow(np, ioc.idx, ioc.val_lo, ioc.ena);
                                /* UserCode{9DB5D93A-0FD5-424b-B6CC-AFC33CF07FEC}:r8srRGlzXT */
                                rc = 0;
                                /* UserCode{9DB5D93A-0FD5-424b-B6CC-AFC33CF07FEC} */
                            }
                            else
                            {

                                if (ioc.cmd == OAK_IOCTL_RXFLOW_DA)
                                {
                                    oak_unimac_set_rx_da(np, ioc.idx, ioc.data, ioc.ena);
                                    /* UserCode{5AA1F9D4-63B4-4fc2-A9F0-D144E9E00A25}:r8srRGlzXT */
                                    rc = 0;
                                    /* UserCode{5AA1F9D4-63B4-4fc2-A9F0-D144E9E00A25} */
                                }
                                else
                                {

                                    if (ioc.cmd == OAK_IOCTL_RXFLOW_DA_MASK && np->pci_class_revision >= 1)
                                    {
                                        oak_unimac_set_rx_da_mask(np, ioc.idx, ioc.data, ioc.ena);
                                        /* UserCode{3465B918-A659-4c15-B5AD-9AB77ACE9026}:r8srRGlzXT */
                                        rc = 0;
                                        /* UserCode{3465B918-A659-4c15-B5AD-9AB77ACE9026} */
                                    }
                                    else
                                    {

                                        if (ioc.cmd == OAK_IOCTL_RXFLOW_FID)
                                        {
                                            oak_unimac_set_rx_8021Q_fid(np, ioc.idx, ioc.val_lo, ioc.ena);
                                            /* UserCode{551464D9-DB6B-4a24-A7B5-008C1E7D50B9}:r8srRGlzXT */
                                            rc = 0;
                                            /* UserCode{551464D9-DB6B-4a24-A7B5-008C1E7D50B9} */
                                        }
                                        else
                                        {

                                            if (ioc.cmd == OAK_IOCTL_RXFLOW_ET)
                                            {

                                                if (ioc.ena != 0)
                                                {
                                                    /* UserCode{F4B179CF-794D-4a47-815B-E66249277271}:1QP6h2XPoR */
                                                    v0 = ioc.val_lo & 0xFFFF;
                                                    v1 = ioc.val_hi & 0xFFFF;
                                                    /* UserCode{F4B179CF-794D-4a47-815B-E66249277271} */
                                                }
                                                else
                                                {
                                                    /* UserCode{24C33268-456D-4be5-8C48-FBE9FFB45774}:U8IcixnmKd */
                                                    v0 = 0;
                                                    v1 = 0;
                                                    /* UserCode{24C33268-456D-4be5-8C48-FBE9FFB45774} */
                                                }
                                                oak_unimac_set_rx_8021Q_et(np, ioc.idx, v0, v1, ioc.ena);
                                                /* UserCode{298DA314-4B48-4c9d-A79C-324E6D8E026F}:r8srRGlzXT */
                                                rc = 0;
                                                /* UserCode{298DA314-4B48-4c9d-A79C-324E6D8E026F} */
                                            }
                                            else
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
        }
        /* UserCode{1DB82AD5-13C8-4ffc-BA22-883CE108688A}:1NPBPpWDbt */
        ioc.error = rc;
        /* UserCode{1DB82AD5-13C8-4ffc-BA22-883CE108688A} */

        /* UserCode{B394B0EF-8932-4b09-B659-4BE2BDC09052}:IOHxKSFByd */
        rc = copy_to_user(ifr->ifr_data, &ioc, sizeof(ioc));
        /* UserCode{B394B0EF-8932-4b09-B659-4BE2BDC09052} */
    }
    else
    {
    }
    /* UserCode{CF832F61-570E-4f40-94AD-DC09FB28D3D1}:hdlNM0yhVZ */
    oakdbg(debug, DRV, "cmd:0x%x ioc=0x%x err=%d", cmd, ioc.cmd, rc);
    /* UserCode{CF832F61-570E-4f40-94AD-DC09FB28D3D1} */

    rc_37 = rc;
    return rc_37;
}

/* Name      : set_txr_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_set_txr_rate(oak_t* np, struct ifreq* ifr, int cmd)
{
    uint32_t reg;
    uint32_t val;
    oak_ioc_set ioc;
    int rc = 0; /* automatically added for object flow handling */
    int rc_7 = rc; /* start of activity code */
    /* UserCode{B4CFD9EA-BB12-4a4e-A1A1-186E31BF61D3}:lYS0x0eI4j */
    rc = copy_from_user(&ioc, ifr->ifr_data, sizeof(ioc));
    /* UserCode{B4CFD9EA-BB12-4a4e-A1A1-186E31BF61D3} */

    if (rc == 0)
    {

        if (cmd == OAK_IOCTL_SET_TXR_RATE)
        {
            /* UserCode{29BAA251-3E50-46a6-99A5-53067C013E2F}:1ERI235gzS */
            reg = OAK_UNI_TX_RING_RATECTRL(ioc.idx);
            /* UserCode{29BAA251-3E50-46a6-99A5-53067C013E2F} */

            val = oak_unimac_io_read_32(np, reg);
            /* UserCode{00FE148C-B47E-4ac2-B31D-8AEF8C79B2B1}:YBZsW0iftP */
            val &= ~0x1FFFF;
            val |= ioc.data & 0x1FFFF;

            /* UserCode{00FE148C-B47E-4ac2-B31D-8AEF8C79B2B1} */

            oak_unimac_io_write_32(np, reg, val);
        }
        else
        {
        }
    }
    else
    {
    }
    rc_7 = rc;
    return rc_7;
}

/* Name      : direct_register_access
 * Returns   : int
 * Parameters:  oak_t * np,  struct ifreq * ifr,  int cmd
 *  */
int oak_ctl_direct_register_access(oak_t* np, struct ifreq* ifr, int cmd)
{
    int reg_timeout = 100;
    int rc;
    uint32_t val;
    oak_ioc_reg req; /* automatically added for object flow handling */
    int rc_27; /* start of activity code */
    /* UserCode{02345228-9903-4361-B05F-71715F7B8115}:l6ARB3E6qd */
    rc = copy_from_user(&req, ifr->ifr_data, sizeof(req));
    /* UserCode{02345228-9903-4361-B05F-71715F7B8115} */

    if (rc == 0)
    {

        if (cmd == OAK_IOCTL_REG_ESU_REQ && req.cmd == OAK_IOCTL_REG_RD)
        {
            req.data = oak_ctl_esu_rd32(np, &req);
            /* UserCode{89984F70-DEC2-4561-9911-3C0F9D08D450}:r8srRGlzXT */
            rc = 0;
            /* UserCode{89984F70-DEC2-4561-9911-3C0F9D08D450} */
        }
        else
        {
        }

        if (cmd == OAK_IOCTL_REG_MAC_REQ && req.cmd == OAK_IOCTL_REG_RD)
        {
            req.data = oak_unimac_io_read_32(np, req.offs);
            /* UserCode{7A8D37F6-D270-4d52-8AD0-2978A51E2609}:r8srRGlzXT */
            rc = 0;
            /* UserCode{7A8D37F6-D270-4d52-8AD0-2978A51E2609} */
        }
        else
        {
        }

        if (cmd == OAK_IOCTL_REG_ESU_REQ && req.cmd == OAK_IOCTL_REG_WR)
        {
            oak_ctl_esu_wr32(np, &req);
            /* UserCode{1E14164F-1ECB-416e-9575-C59AF5B918F9}:r8srRGlzXT */
            rc = 0;
            /* UserCode{1E14164F-1ECB-416e-9575-C59AF5B918F9} */
        }
        else
        {
        }

        if (cmd == OAK_IOCTL_REG_MAC_REQ && req.cmd == OAK_IOCTL_REG_WR)
        {
            oak_unimac_io_write_32(np, req.offs, req.data);
            /* UserCode{192FCA94-44BC-416e-9FC0-53B3E67767D6}:r8srRGlzXT */
            rc = 0;
            /* UserCode{192FCA94-44BC-416e-9FC0-53B3E67767D6} */
        }
        else
        {
        }

        if (req.cmd == OAK_IOCTL_REG_WC)
        {
            do
            {

                if (cmd == OAK_IOCTL_REG_ESU_REQ)
                {
                    val = oak_ctl_esu_rd32(np, &req);
                }
                else
                {
                    val = oak_unimac_io_read_32(np, req.offs);
                }
            } while ((--reg_timeout > 0) && ((val & (1 << req.data)) != 0));

            if (reg_timeout == 0)
            {
                /* UserCode{DD925FEE-BF13-4883-B14D-4FD4D79941CB}:tGbLT2rByH */
                rc = -EFAULT;
                /* UserCode{DD925FEE-BF13-4883-B14D-4FD4D79941CB} */
            }
            else
            {
                /* UserCode{A45BA98D-F2BE-4f62-955A-E5921C5D0AD2}:r8srRGlzXT */
                rc = 0;
                /* UserCode{A45BA98D-F2BE-4f62-955A-E5921C5D0AD2} */
            }
        }
        else
        {
        }

        if (req.cmd == OAK_IOCTL_REG_WS)
        {
            /* UserCode{1C88F2C9-E848-45c9-A475-6EFE6C694C3A}:AY8ytJtEEA */
            reg_timeout = 100;
            /* UserCode{1C88F2C9-E848-45c9-A475-6EFE6C694C3A} */

            do
            {

                if (cmd == OAK_IOCTL_REG_ESU_REQ)
                {
                    val = oak_ctl_esu_rd32(np, &req);
                }
                else
                {
                    val = oak_unimac_io_read_32(np, req.offs);
                }
            } while ((--reg_timeout > 0) && ((val & (1 << req.data)) == 0));

            if (reg_timeout == 0)
            {
                /* UserCode{40C5AD02-DB77-40b4-B266-C0A27943F0EC}:tGbLT2rByH */
                rc = -EFAULT;
                /* UserCode{40C5AD02-DB77-40b4-B266-C0A27943F0EC} */
            }
            else
            {
                /* UserCode{408CB6B1-B0DE-4191-A185-87ADA2950009}:r8srRGlzXT */
                rc = 0;
                /* UserCode{408CB6B1-B0DE-4191-A185-87ADA2950009} */
            }
        }
        else
        {
        }
        /* UserCode{82403923-7F6B-4555-8A0D-044A78B958F9}:gRYJRUVnrn */
        oakdbg(debug, DRV, "MAC RD at offset: 0x%x data=0x%x err=%d", req.offs, req.data, rc);
        /* UserCode{82403923-7F6B-4555-8A0D-044A78B958F9} */

        /* UserCode{DEFC1231-3CCD-4414-8FA4-319C7446ED90}:UkatlF1Wx3 */
        req.error = rc;
        rc = copy_to_user(ifr->ifr_data, &req, sizeof(req));
        /* UserCode{DEFC1231-3CCD-4414-8FA4-319C7446ED90} */
    }
    else
    {
        /* UserCode{F8468CF2-1C9F-42c2-B726-9707E94FA621}:tGbLT2rByH */
        rc = -EFAULT;
        /* UserCode{F8468CF2-1C9F-42c2-B726-9707E94FA621} */
    }
    rc_27 = rc;
    /* UserCode{38A57CE5-C23F-4c06-B42F-9B8B3A6FFA93}:e6dJTa0pdt */
    oakdbg(debug, DRV, "cmd=0x%xreq=0x%x rc=%d", cmd, req.cmd, rc);
    /* UserCode{38A57CE5-C23F-4c06-B42F-9B8B3A6FFA93} */

    return rc_27;
}

/* Name      : load_generator_access
 * Returns   : int
 * Parameters:  oak_t * np,  struct ifreq * ifr,  int cmd
 *  */
int oak_ctl_load_generator_access(oak_t* np, struct ifreq* ifr, int cmd)
{
    oak_ioc_lgen req;
    int rc; /* automatically added for object flow handling */
    int rc_19 = -EINVAL; /* start of activity code */
    /* UserCode{B4FD024D-F73D-402c-8D98-81F7B9740636}:l6ARB3E6qd */
    rc = copy_from_user(&req, ifr->ifr_data, sizeof(req));
    /* UserCode{B4FD024D-F73D-402c-8D98-81F7B9740636} */

    if (rc == 0)
    {

        if (cmd == OAK_IOCTL_LGEN)
        {

            if (np->level < 44)
            {
                /* UserCode{92C65B71-29A3-437a-AAE2-EC8A42CF0469}:pyZyUQzw1j */
                req.error = -EFAULT;
                /* UserCode{92C65B71-29A3-437a-AAE2-EC8A42CF0469} */
            }
            else
            {

                if ((req.cmd & OAK_IOCTL_LGEN_INIT) == OAK_IOCTL_LGEN_INIT)
                {
                    rc_19 = oak_ctl_lgen_init(np, &req);
                }
                else
                {
                }

                if ((req.cmd & OAK_IOCTL_LGEN_TX_DATA) == OAK_IOCTL_LGEN_TX_DATA)
                {
                    rc_19 = oak_ctl_lgen_tx_data(np, &req);
                }
                else
                {
                }

                if ((req.cmd & OAK_IOCTL_LGEN_TX_START) == OAK_IOCTL_LGEN_TX_START)
                {
                    rc_19 = oak_ctl_lgen_tx_start(np, &req);
                }
                else
                {
                }

                if ((req.cmd & OAK_IOCTL_LGEN_TX_STOP) == OAK_IOCTL_LGEN_TX_STOP)
                {
                    rc_19 = oak_ctl_lgen_tx_stop(np, &req);
                }
                else
                {
                }

                if ((req.cmd & OAK_IOCTL_LGEN_RX_START) == OAK_IOCTL_LGEN_RX_START)
                {
                    rc_19 = oak_ctl_lgen_rx_start(np, &req);
                }
                else
                {
                }

                if ((req.cmd & OAK_IOCTL_LGEN_RX_STOP) == OAK_IOCTL_LGEN_RX_STOP)
                {
                    rc_19 = oak_ctl_lgen_rx_stop(np, &req);
                }
                else
                {
                }

                if ((req.cmd & OAK_IOCTL_LGEN_RX_DATA) == OAK_IOCTL_LGEN_RX_DATA)
                {
                    rc_19 = oak_ctl_lgen_rx_data(np, &req);
                }
                else
                {
                }

                if ((req.cmd & OAK_IOCTL_LGEN_RELEASE) == OAK_IOCTL_LGEN_RELEASE)
                {
                    rc_19 = oak_ctl_lgen_release(np, &req);
                }
                else
                {
                }

                if ((((req.cmd & OAK_IOCTL_LGEN_RX_RESET) == OAK_IOCTL_LGEN_RX_RESET || (req.cmd & OAK_IOCTL_LGEN_TX_RESET) == OAK_IOCTL_LGEN_TX_RESET)))
                {
                    rc_19 = oak_ctl_lgen_txrx_reset(np, &req);
                }
                else
                {
                }
            }
            /* UserCode{8373EE5E-E66D-4581-96C2-A1DCA077336E}:Abhq8Z9WW7 */
            rc = copy_to_user(ifr->ifr_data, &req, sizeof(req));
            /* UserCode{8373EE5E-E66D-4581-96C2-A1DCA077336E} */

            if (rc != 0)
            {
                rc_19 = rc;
            }
            else
            {
            }
        }
        else
        {
        }
    }
    else
    {
        rc_19 = rc;
    }
    /* UserCode{0C2957BE-FF61-4188-ABA7-AA8822EBC37C}:T7ELqq2Eec */
    oakdbg(debug, DRV, "np-level=%d cmd=0x%x req=0x%x rc=%d", np->level, cmd, req.cmd, rc);
    /* UserCode{0C2957BE-FF61-4188-ABA7-AA8822EBC37C} */

    return rc_19;
}

/* Name      : lgen_init
 * Returns   : int
 * Parameters:  oak_t * np = np,  oak_ioc_lgen * req = req
 *  */
static int oak_ctl_lgen_init(oak_t* np, oak_ioc_lgen* req)
{
    /* automatically added for object flow handling */
    int rc_2; /* start of activity code */
    /* UserCode{E09CFC50-B90F-4332-A208-ABE829C63AC1}:13f8ynJmni */
    netif_carrier_off(np->netdev);
    /* UserCode{E09CFC50-B90F-4332-A208-ABE829C63AC1} */

    oak_net_stop_all(np);
    /* UserCode{009A9938-5C63-445e-93D4-0BF9142BE737}:1HXbZmQyni */
    np->level = 45;
    /* UserCode{009A9938-5C63-445e-93D4-0BF9142BE737} */

    rc_2 = 0;
    return rc_2;
}

/* Name      : lgen_release
 * Returns   : int
 * Parameters:  oak_t * np = np,  oak_ioc_lgen * req = req
 *  */
static int oak_ctl_lgen_release(oak_t* np, oak_ioc_lgen* req)
{
    /* automatically added for object flow handling */
    int rc_1; /* start of activity code */

    if (np->level < 45)
    {
        rc_1 = -EINVAL;
    }
    else
    {
        rc_1 = oak_ctl_lgen_init(np, req);
        /* UserCode{1A09BDFD-C91C-4062-AC63-2575E91A1EB6}:KDCXMOMlrx */
        np->level = 44;
        netif_carrier_on(np->netdev);
        /* UserCode{1A09BDFD-C91C-4062-AC63-2575E91A1EB6} */
    }
    return rc_1;
}

/* Name      : lgen_rx_data
 * Returns   : int
 * Parameters:  oak_t * np = np,  oak_ioc_lgen * req = req
 *  */
static int oak_ctl_lgen_rx_data(oak_t* np, oak_ioc_lgen* req)
{
    struct page* page = NULL;
    oak_rx_chan_t* rxc;
    oak_rxd_t* rxd;
    oak_rxs_t* rxs;
    oak_rxa_t* rba;
    caddr_t va;
    int rc; /* automatically added for object flow handling */
    int rc_1; /* start of activity code */

    if (req->channel < (sizeof(np->rx_channel) / sizeof(oak_rx_chan_t)))
    {
        /* UserCode{D48E4620-F5E4-4b46-B991-40A2A54382D6}:lkZsv4fPnU */
        rxc = &np->rx_channel[req->channel];
        /* UserCode{D48E4620-F5E4-4b46-B991-40A2A54382D6} */

        if (req->count < rxc->rbr_size)
        {
            /* UserCode{93BFB447-9578-45df-BF58-75D157AA2396}:PcX1THWiCf */
            rxd = &rxc->rbr[req->count];
            rxs = &rxc->rsr[req->count];
            rba = &rxc->rba[req->count];
            /* UserCode{93BFB447-9578-45df-BF58-75D157AA2396} */

            if (req->len + req->offs > rxs->bc)
            {
                /* UserCode{AE51127E-FA56-4563-9223-4F7D6C725686}:1TfQ5QIlaK */
                req->len = rxs->bc - req->offs;
                /* UserCode{AE51127E-FA56-4563-9223-4F7D6C725686} */
            }
            else
            {
            }
            /* UserCode{662C3261-478A-4d3e-B989-8925013AB067}:23HhcaSwFe */
            page = rba->page_virt;
            /* UserCode{662C3261-478A-4d3e-B989-8925013AB067} */

            if (page == NULL)
            {
                /* UserCode{493CC265-0969-4331-875C-6085431A6F02}:17zylXIV9A */
                rc = -ENOMEM;
                /* UserCode{493CC265-0969-4331-875C-6085431A6F02} */
            }
            else
            {
                /* UserCode{BFCBFCFA-4C00-4dc9-A36A-09EF234A5B18}:19xssjxJkj */
                va = page_address(page);
                if (req->len > sizeof(req->data))
                {
                    req->len = sizeof(req->data);
                }
                memcpy(req->data, va + rba->page_offs + req->offs, req->len);

                /* UserCode{BFCBFCFA-4C00-4dc9-A36A-09EF234A5B18} */

                if (req->len > sizeof(req->data))
                {
                    /* UserCode{1A1E81E8-76D5-473c-8169-CBF0CB0998C9}:G9Ah0gSp6y */
                    req->len = sizeof(req->data);
                    /* UserCode{1A1E81E8-76D5-473c-8169-CBF0CB0998C9} */
                }
                else
                {
                }
                /* UserCode{3C0BD9A7-B327-45e7-A4B0-194F39E334C3}:3IuYAgL688 */
                memcpy(req->data, va + rba->page_offs + req->offs, req->len);
                /* UserCode{3C0BD9A7-B327-45e7-A4B0-194F39E334C3} */

                /* UserCode{5D179B6A-0759-4729-98DD-5E57693A2FE1}:r8srRGlzXT */
                rc = 0;
                /* UserCode{5D179B6A-0759-4729-98DD-5E57693A2FE1} */
            }
        }
        else
        {
            /* UserCode{63E3EED6-D82E-4013-9BB3-490C66B09769}:FubMmJTDKN */
            rc = -EINVAL;
            /* UserCode{63E3EED6-D82E-4013-9BB3-490C66B09769} */
        }
    }
    else
    {
        /* UserCode{2EE0224F-0F44-4ac9-99DD-5EFBE701E04A}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{2EE0224F-0F44-4ac9-99DD-5EFBE701E04A} */
    }
    /* UserCode{B0870048-20FB-4d14-A144-F908DD1AFD29}:1KB016l8IK */
    req->error = rc;
    /* UserCode{B0870048-20FB-4d14-A144-F908DD1AFD29} */

    rc_1 = rc;
    /* UserCode{7B46C34A-A1F5-4780-957D-C7ED7162DA64}:1TwStmcqMW */
    oakdbg(debug, DRV, "req.cmd=0x%x tx_ring=%d offs=0x%x len=%d count=%d page=%p rc=%d", req->cmd, req->channel, req->offs, req->len, req->count, page, rc);

    /* UserCode{7B46C34A-A1F5-4780-957D-C7ED7162DA64} */

    return rc_1;
}

/* Name      : lgen_rx_stop
 * Returns   : int
 * Parameters:  oak_t * np = np,  oak_ioc_lgen * req = req
 *  */
static int oak_ctl_lgen_rx_stop(oak_t* np, oak_ioc_lgen* req)
{
    uint32_t count;
    uint32_t idx;
    oak_rx_chan_t* rxc;
    int rc; /* automatically added for object flow handling */
    int rc_1; /* start of activity code */

    if (np->level < 45)
    {
        /* UserCode{C51D6C1C-DDA2-45b4-9FC2-57CF8F246474}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{C51D6C1C-DDA2-45b4-9FC2-57CF8F246474} */
    }
    else
    {

        if (req->channel >= np->num_rx_chan)
        {
            /* UserCode{D2B17621-ED3F-4437-971E-8B61A9F8A806}:1Hu3EQjDKl */
            count = np->num_rx_chan;
            idx = 0;

            /* UserCode{D2B17621-ED3F-4437-971E-8B61A9F8A806} */
        }
        else
        {
            /* UserCode{7402A3A3-B8D7-411e-8339-43EC6D1BF03B}:17ecMRABoY */
            count = 1;
            idx = req->channel;

            /* UserCode{7402A3A3-B8D7-411e-8339-43EC6D1BF03B} */
        }
        do
        {
            /* UserCode{3AD13D00-560C-4381-9201-1FB96983B828}:oROazkeBa2 */
            rxc = &np->rx_channel[idx];
            rc = oak_unimac_start_rx_ring(np, idx, 0);
            /* UserCode{3AD13D00-560C-4381-9201-1FB96983B828} */

            if (rc > 0)
            {
                /* UserCode{B3723AF8-DD16-4d71-88B2-16309D170B6D}:13KsEnRxdt */
                rxc->flags = 0;
                ++idx;
                --count;
                rc = 0;
                /* UserCode{B3723AF8-DD16-4d71-88B2-16309D170B6D} */
            }
            else
            {
                /* UserCode{5C98BF4A-C726-4643-A072-FD36B3418259}:WyHqPMY9K4 */
                rc = -EFAULT;
                break;

                /* UserCode{5C98BF4A-C726-4643-A072-FD36B3418259} */
            }
        } while (count > 0);
    }
    /* UserCode{CACBDC03-32BD-4405-91FD-19CE70CC4F55}:1Lm0ecRniW */
    oakdbg(debug, DRV, "np=%p chan=%d rc=%d", np, req->channel, rc);
    /* UserCode{CACBDC03-32BD-4405-91FD-19CE70CC4F55} */

    rc_1 = rc;
    return rc_1;
}

/* Name      : lgen_rx_start
 * Returns   : int
 * Parameters:  oak_t * np = np,  oak_ioc_lgen * req = req
 *  */
static int oak_ctl_lgen_rx_start(oak_t* np, oak_ioc_lgen* req)
{
    uint32_t count;
    uint32_t idx;
    oak_rx_chan_t* rxc;
    uint32_t rbr_pend;
    int rc; /* automatically added for object flow handling */
    int rc_4; /* start of activity code */

    if (np->level < 45)
    {
        /* UserCode{C8D94DDB-B891-4c6a-B9A7-2AF5E8B1E688}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{C8D94DDB-B891-4c6a-B9A7-2AF5E8B1E688} */
    }
    else
    {
        /* UserCode{FF203F6F-2C14-423e-AFA7-1567E8E27036}:ipd4zILkdf */
        oak_irq_ena_general(np, 1);
        /* UserCode{FF203F6F-2C14-423e-AFA7-1567E8E27036} */

        if (req->channel >= np->num_rx_chan)
        {
            /* UserCode{21801714-CE3C-4f62-A5B6-C4BDE640A051}:1Hu3EQjDKl */
            count = np->num_rx_chan;
            idx = 0;

            /* UserCode{21801714-CE3C-4f62-A5B6-C4BDE640A051} */
        }
        else
        {
            /* UserCode{BD9F05E7-FA73-4844-8EC8-D8AC98A1FDCC}:17ecMRABoY */
            count = 1;
            idx = req->channel;
            /* UserCode{BD9F05E7-FA73-4844-8EC8-D8AC98A1FDCC} */
        }
        do
        {
            /* UserCode{89EF8512-AC9F-4334-BD00-72CBE04BC381}:miCRXSDuVQ */
            rxc = &np->rx_channel[idx];
            /* UserCode{89EF8512-AC9F-4334-BD00-72CBE04BC381} */

            rc = oak_net_rbr_refill(np, idx);

            if (rc != 0)
            {
                /* UserCode{1A7B74C3-85B6-4f91-B501-F01AC5AA09F4}:WyHqPMY9K4 */
                rc = -EFAULT;
                break;
                /* UserCode{1A7B74C3-85B6-4f91-B501-F01AC5AA09F4} */
            }
            else
            {
                /* UserCode{135EC475-24BD-477c-BE2B-107CB67CA3BF}:8AEFaWrUFY */
                rbr_pend = atomic_read(&rxc->rbr_pend);
                /* UserCode{135EC475-24BD-477c-BE2B-107CB67CA3BF} */

                if (req->count <= rbr_pend)
                {
                }
                else
                {

                    if (rbr_pend == rxc->rbr_size)
                    {
                    }
                    else
                    {
                        /* UserCode{754D4AC8-14EE-4529-8CE8-EA777AB6CEB0}:3MpL7C18Dp */
                        req->count = rbr_pend;
                        /* UserCode{754D4AC8-14EE-4529-8CE8-EA777AB6CEB0} */
                    }
                }
                /* UserCode{2135F28E-3D5E-494a-A1A2-B50A8185B0F8}:1S0e0CMbZX */
                rc = oak_unimac_start_rx_ring(np, idx, 1);
                ++idx;
                --count;
                /* UserCode{2135F28E-3D5E-494a-A1A2-B50A8185B0F8} */

                if (rc > 0)
                {
                    /* UserCode{383D5317-EC0B-4c3f-B60D-DDCA80C3A6D6}:xb8SB7wdn8 */
                    rxc->rbr_count = req->count;
                    rxc->flags = OAK_RX_LGEN_RX_MODE;
                    rc = 0;
                    /* UserCode{383D5317-EC0B-4c3f-B60D-DDCA80C3A6D6} */
                }
                else
                {
                    /* UserCode{F241866D-BBC2-48e9-B849-D5A9DD846C6E}:WyHqPMY9K4 */
                    rc = -EFAULT;
                    break;
                    /* UserCode{F241866D-BBC2-48e9-B849-D5A9DD846C6E} */
                }
            }
        } while (count > 0);
    }
    /* UserCode{30B8A6CF-81B1-4f0a-9B6C-26D473AF2826}:1Lm0ecRniW */
    oakdbg(debug, DRV, "np=%p chan=%d rc=%d", np, req->channel, rc);
    /* UserCode{30B8A6CF-81B1-4f0a-9B6C-26D473AF2826} */

    rc_4 = rc;
    return rc_4;
}

/* Name      : lgen_tx_start
 * Returns   : int
 * Parameters:  oak_t * np,  oak_ioc_lgen * req
 *  */
static int oak_ctl_lgen_tx_start(oak_t* np, oak_ioc_lgen* req)
{
    uint32_t count;
    uint32_t idx;
    oak_tx_chan_t* txc;
    int rc; /* automatically added for object flow handling */
    int rc_5; /* start of activity code */

    if (np->level < 45)
    {
        /* UserCode{43BD6EB6-D8A0-4e58-999A-2FF1420F6614}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{43BD6EB6-D8A0-4e58-999A-2FF1420F6614} */
    }
    else
    {
        /* UserCode{610440F5-376C-4dd7-A4DD-D36FE1F09A90}:ipd4zILkdf */
        oak_irq_ena_general(np, 1);
        /* UserCode{610440F5-376C-4dd7-A4DD-D36FE1F09A90} */

        if (req->channel >= np->num_tx_chan)
        {
            /* UserCode{3C60C032-E6BE-483f-9D94-AEA8A5F71773}:163Ur2vQeY */
            count = np->num_tx_chan;
            idx = 0;

            /* UserCode{3C60C032-E6BE-483f-9D94-AEA8A5F71773} */
        }
        else
        {
            /* UserCode{A4F7E45E-03D6-42f7-ACE4-B02D111467CF}:17ecMRABoY */
            count = 1;
            idx = req->channel;
            /* UserCode{A4F7E45E-03D6-42f7-ACE4-B02D111467CF} */
        }
        /* UserCode{079B1202-C2C2-4500-9139-7B579A59C073}:4FWMn4S9TN */
        wmb();
        /* UserCode{079B1202-C2C2-4500-9139-7B579A59C073} */

        do
        {
            /* UserCode{9D3C3A81-FD4E-43bd-AAE5-F20BDB5207A1}:ENEulYXrbA */
            txc = &np->tx_channel[idx];
            /* UserCode{9D3C3A81-FD4E-43bd-AAE5-F20BDB5207A1} */

            if ((txc->flags & OAK_RX_LGEN_TX_MODE) == 0)
            {
                /* UserCode{9CBCC738-DD36-47d5-84F0-38B79183754E}:zMGCV3oWDX */
                rc = oak_unimac_start_tx_ring(np, idx, 1);
                /* UserCode{9CBCC738-DD36-47d5-84F0-38B79183754E} */
            }
            else
            {
                /* UserCode{CE17FA91-BCCF-4c38-A11F-EB29F8F09454}:ZuwgoXTLKt */
                rc = 1;
                /* UserCode{CE17FA91-BCCF-4c38-A11F-EB29F8F09454} */
            }

            if (rc > 0)
            {
                /* UserCode{D37B6164-EC52-40f1-8AA4-BF193491F0D3}:vmevBWKfVM */
                txc->tbr_count = req->count;
                txc->tbr_compl = 0;
                /* UserCode{D37B6164-EC52-40f1-8AA4-BF193491F0D3} */

                oak_ctl_lgen_xmit_frame(np, txc, idx, req->count);
                /* UserCode{ABE7CF60-8641-40ab-AAAF-825F051F490E}:c24CyuE0dP */
                rc = 0;
                ++idx;
                --count;

                /* UserCode{ABE7CF60-8641-40ab-AAAF-825F051F490E} */
            }
            else
            {
                /* UserCode{F41E6133-CEAE-460f-913D-41A38AA4D7AD}:WyHqPMY9K4 */
                rc = -EFAULT;
                break;
                /* UserCode{F41E6133-CEAE-460f-913D-41A38AA4D7AD} */
            }
        } while (count > 0);
    }
    /* UserCode{F1215D2D-0FAC-482f-8B92-B2F317D75B7F}:1Lm0ecRniW */
    oakdbg(debug, DRV, "np=%p chan=%d rc=%d", np, req->channel, rc);
    /* UserCode{F1215D2D-0FAC-482f-8B92-B2F317D75B7F} */

    rc_5 = rc;
    return rc_5;
}

/* Name      : lgen_tx_stop
 * Returns   : int
 * Parameters:  oak_t * np,  oak_ioc_lgen * req
 *  */
static int oak_ctl_lgen_tx_stop(oak_t* np, oak_ioc_lgen* req)
{
    uint32_t count;
    uint32_t idx;
    oak_tx_chan_t* txc;
    int rc; /* automatically added for object flow handling */
    int rc_1; /* start of activity code */

    if (np->level < 45)
    {
        /* UserCode{FF4E7E75-A6CC-4259-8667-53C4EF9BC733}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{FF4E7E75-A6CC-4259-8667-53C4EF9BC733} */
    }
    else
    {

        if (req->channel >= np->num_tx_chan)
        {
            /* UserCode{3BA16E9D-709D-4ebe-829C-786EC3620B18}:163Ur2vQeY */
            count = np->num_tx_chan;
            idx = 0;
            /* UserCode{3BA16E9D-709D-4ebe-829C-786EC3620B18} */
        }
        else
        {
            /* UserCode{EC2C2A14-FBD4-4a90-89F6-E49ECA8E2470}:17ecMRABoY */
            count = 1;
            idx = req->channel;
            /* UserCode{EC2C2A14-FBD4-4a90-89F6-E49ECA8E2470} */
        }
        do
        {
            /* UserCode{B33D81E9-A238-4828-B860-25BC8DEF8F29}:ENEulYXrbA */
            txc = &np->tx_channel[idx];
            /* UserCode{B33D81E9-A238-4828-B860-25BC8DEF8F29} */

            /* UserCode{310F024E-8A09-48b9-8D62-99E6D2B7C01A}:nAUBUYAJ9C */
            rc = oak_unimac_start_tx_ring(np, idx, 0);

            /* UserCode{310F024E-8A09-48b9-8D62-99E6D2B7C01A} */

            if (rc > 0)
            {
                /* UserCode{D3FAA9F4-1ABD-4c5e-9AFB-A833EFCC9E62}:1AyGloDNf3 */
                txc->flags &= ~OAK_RX_LGEN_TX_MODE;
                txc->flags = 0;
                ++idx;
                --count;
                rc = 0;

                /* UserCode{D3FAA9F4-1ABD-4c5e-9AFB-A833EFCC9E62} */
            }
            else
            {
                /* UserCode{954459AE-319A-4415-9D15-388E9B4FBADF}:WyHqPMY9K4 */
                rc = -EFAULT;
                break;
                /* UserCode{954459AE-319A-4415-9D15-388E9B4FBADF} */
            }
        } while (count > 0);
    }
    /* UserCode{33A9FDE1-2188-49f4-9209-A635D112D624}:1Lm0ecRniW */
    oakdbg(debug, DRV, "np=%p chan=%d rc=%d", np, req->channel, rc);
    /* UserCode{33A9FDE1-2188-49f4-9209-A635D112D624} */

    rc_1 = rc;
    return rc_1;
}

/* Name      : lgen_tx_data
 * Returns   : int
 * Parameters:  oak_t * np = np,  oak_ioc_lgen * req = req
 *  */
static int oak_ctl_lgen_tx_data(oak_t* np, oak_ioc_lgen* req)
{
    struct page* page = NULL;
    dma_addr_t dma = 0;
    oak_tx_chan_t* txc = NULL;
    int flags = 0;
    uint32_t count;
    uint32_t free_desc;
    uint32_t cnt;
    int rc = -ENOMEM; /* automatically added for object flow handling */
    int rc_29; /* start of activity code */

    if (np->level < 45)
    {
        /* UserCode{537C927D-45F1-4ca3-94F8-C5C1C34F7663}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{537C927D-45F1-4ca3-94F8-C5C1C34F7663} */
    }
    else
    {

        if (req->channel < (sizeof(np->tx_channel) / sizeof(oak_tx_chan_t)))
        {
            /* UserCode{D7447D6F-DFEA-4525-A7C7-66E103DB4324}:1BX3vDyuH4 */
            txc = &np->tx_channel[req->channel];
#if 1
            free_desc = atomic_read(&txc->tbr_pend);
            free_desc = txc->tbr_size - free_desc;
#else
            free_desc = txc->tbr_size - txc->tbr_pend;
#endif
            /* UserCode{D7447D6F-DFEA-4525-A7C7-66E103DB4324} */

            if ((txc->flags & OAK_RX_LGEN_TX_MODE) == 0 && (free_desc >= req->count || req->offs > 0))
            {
                /* UserCode{6294774D-C749-4c84-A22A-1D78078878FB}:1CmzUcekQi */
                cnt = atomic_read(&txc->tbr_pend);
                /* UserCode{6294774D-C749-4c84-A22A-1D78078878FB} */

                if (req->offs > 0 && cnt > 0)
                {
                    /* UserCode{44A76E30-88F1-4f71-A045-8CED2D4A7539}:qIBUfwZNY7 */
                    uint32_t idx = (txc->tbr_widx == 0) ? txc->tbr_size - 1 : txc->tbr_widx - 1;
                    page = txc->tbi[idx].page;
                    /* UserCode{44A76E30-88F1-4f71-A045-8CED2D4A7539} */
                }
                else
                {
                    page = oak_net_alloc_page(np, &dma, DMA_TO_DEVICE);
                }

                if (page != NULL && req->count > 0)
                {
                    /* UserCode{8BC0BD1B-B73B-47a7-9A33-A430862D7D9A}:Fyx4WQKXSl */
                    caddr_t va = page_address(page);
                    /* UserCode{8BC0BD1B-B73B-47a7-9A33-A430862D7D9A} */

                    /* UserCode{066E2E79-AEA5-40ba-91D8-4E7E2CC98312}:1CmzUcekQi */
                    cnt = atomic_read(&txc->tbr_pend);
                    /* UserCode{066E2E79-AEA5-40ba-91D8-4E7E2CC98312} */

                    if ((dma == 0) && (cnt < req->count))
                    {
                        /* UserCode{DB6BE463-6CAA-4855-B010-75668F671DA6}:FubMmJTDKN */
                        rc = -EINVAL;
                        /* UserCode{DB6BE463-6CAA-4855-B010-75668F671DA6} */
                    }
                    else
                    {

                        if (dma == 0)
                        {
                            /* UserCode{465ADAB6-E6CB-453a-8046-FC90F0F5C0FF}:1YgHc9mBYi */
                            txc->tbr_widx = (txc->tbr_widx >= req->count) ? txc->tbr_widx - req->count : txc->tbr_widx + txc->tbr_size - req->count;
#if 1
                            atomic_sub(req->count, &txc->tbr_pend);
#else
                            txc->tbr_pend -= req->count;
#endif
                            /* UserCode{465ADAB6-E6CB-453a-8046-FC90F0F5C0FF} */

                            oak_net_add_txd_length(txc, req->len);
                        }
                        else
                        {
                            /* UserCode{3FBDCDE8-6100-4c49-B7DE-16C917AA93A3}:mGI3chYHoM */
                            flags = (req->count == 1) ? TX_BUFF_INFO_ADR_MAPP : 0;

                            /* UserCode{3FBDCDE8-6100-4c49-B7DE-16C917AA93A3} */

                            oak_net_set_txd_first(txc, req->len, 0, 0, dma, np->page_size, flags);
                        }
                        /* UserCode{40F8871D-5994-45de-87CB-97024B1C86F5}:1EpO4GOsQv */
                        memcpy(va + req->offs, req->data, req->len);
                        count = req->count - 1;
                        /* UserCode{40F8871D-5994-45de-87CB-97024B1C86F5} */

                        while (count > 0)
                        {
                            /* UserCode{8384151E-E2D9-48a0-AFDE-E460D7CCC97D}:111vSG1Aka */
                            txc->tbr_widx = NEXT_IDX(txc->tbr_widx, txc->tbr_size);
#if 1
                            atomic_inc(&txc->tbr_pend);
#else
                            ++txc->tbr_pend;
#endif
                            /* UserCode{8384151E-E2D9-48a0-AFDE-E460D7CCC97D} */

                            if (dma == 0)
                            {
                                oak_net_add_txd_length(txc, req->len);
                            }
                            else
                            {
                                /* UserCode{424039C8-8BCD-4c6d-AFAE-D643CCEA82A9}:1MMvdfxlEI */
                                flags = (count == 1) ? TX_BUFF_INFO_ADR_MAPP : 0;
                                /* UserCode{424039C8-8BCD-4c6d-AFAE-D643CCEA82A9} */

                                oak_net_set_txd_page(txc, req->len, dma, np->page_size, flags);
                            }
                            /* UserCode{6D27EF13-329E-4f8d-B9A9-C02CE04F7E81}:9wEChOWkbj */
                            --count;
                            /* UserCode{6D27EF13-329E-4f8d-B9A9-C02CE04F7E81} */
                        }

                        oak_net_set_txd_last(txc, NULL, page);
                        /* UserCode{63588130-9C05-4cb5-AB69-1E1616B6CCCD}:1TxURSkuOM */
                        txc->tbr_widx = NEXT_IDX(txc->tbr_widx, txc->tbr_size);
#if 1
                        atomic_inc(&txc->tbr_pend);
#else
                        ++txc->tbr_pend;
#endif
                        rc = 0;
                        /* UserCode{63588130-9C05-4cb5-AB69-1E1616B6CCCD} */
                    }
                }
                else
                {
                }
            }
            else
            {
                /* UserCode{9B38A84B-F79F-4b53-ACB6-F8E41D38B27B}:FubMmJTDKN */
                rc = -EINVAL;
                /* UserCode{9B38A84B-F79F-4b53-ACB6-F8E41D38B27B} */
            }
        }
        else
        {
            /* UserCode{67D7FB60-3171-4645-A4E6-22C0CC98BC3C}:FubMmJTDKN */
            rc = -EINVAL;
            /* UserCode{67D7FB60-3171-4645-A4E6-22C0CC98BC3C} */
        }
    }
    /* UserCode{D862D77B-1AE5-4e4b-8A5C-ABC4E7561BB4}:1KB016l8IK */
    req->error = rc;
    /* UserCode{D862D77B-1AE5-4e4b-8A5C-ABC4E7561BB4} */

    rc_29 = rc;
    /* UserCode{C36E92FF-49E2-4df5-9E8B-3A199F9D58ED}:1a8HLuxHmZ */
    oakdbg(debug, DRV, "req.cmd=0x%x tx_ring=%d offs=0x%x len=%d count=%d rc=%d", req->cmd, req->channel, req->offs, req->len, req->count, rc);

    /* UserCode{C36E92FF-49E2-4df5-9E8B-3A199F9D58ED} */

    return rc_29;
}

/* Name      : lgen_txrx_reset
 * Returns   : int
 * Parameters:  oak_t * np = np,  oak_ioc_lgen * req = req
 *  */
static int oak_ctl_lgen_txrx_reset(oak_t* np, oak_ioc_lgen* req)
{
    int rc = 0; /* automatically added for object flow handling */
    int rc_3; /* start of activity code */

    if (np->level < 45)
    {
        /* UserCode{1876DE6B-3C17-44b7-B699-4F88B9A6E593}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{1876DE6B-3C17-44b7-B699-4F88B9A6E593} */
    }
    else
    {

        if (req->cmd == OAK_IOCTL_LGEN_RX_RESET)
        {

            if (req->channel >= np->num_rx_chan)
            {
                /* UserCode{1C5DB9A6-EA80-433d-9EE1-A2117E8085E5}:FubMmJTDKN */
                rc = -EINVAL;
                /* UserCode{1C5DB9A6-EA80-433d-9EE1-A2117E8085E5} */
            }
            else
            {
                oak_net_rbr_free(&np->rx_channel[req->channel]);
                /* UserCode{3EB0E9AD-580C-48ac-92A8-473797CCEBA0}:1Yrl6Yp5BF */
                np->rx_channel[req->channel].flags = 0;
                /* UserCode{3EB0E9AD-580C-48ac-92A8-473797CCEBA0} */
            }
        }
        else
        {

            if (req->channel >= np->num_tx_chan)
            {
                /* UserCode{65446B21-3552-4bea-8622-733C1FC67501}:FubMmJTDKN */
                rc = -EINVAL;
                /* UserCode{65446B21-3552-4bea-8622-733C1FC67501} */
            }
            else
            {
                oak_net_tbr_free(&np->tx_channel[req->channel]);
                /* UserCode{619B4C2E-B840-47c2-97D1-45A988224940}:oisP7ICWsk */
                np->tx_channel[req->channel].flags = 0;
                /* UserCode{619B4C2E-B840-47c2-97D1-45A988224940} */
            }
        }
    }
    rc_3 = rc;
    return rc_3;
}

