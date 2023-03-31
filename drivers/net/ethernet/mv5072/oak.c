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
 * Contents: functions as called from Linux kernel (entry points)
 *
 * Path  : OAK-Linux::OAK Switch Board - Linux Driver::Class Model::oak
 * Author: afischer
 * Date  :2020-09-10  - 12:10
 *  */
#include "oak.h"

static const char oak_driver_name[] = OAK_DRIVER_NAME;
static const char oak_driver_string[] = OAK_DRIVER_STRING;
static const char oak_driver_version[] = OAK_DRIVER_VERSION;
static const char oak_driver_copyright[] = OAK_DRIVER_COPYRIGHT;
static struct pci_device_id oak_pci_tbl[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_SYSKONNECT, 0x1000)},
    {PCI_DEVICE(0x11AB, 0x0000)}, /* FPGA board */
    {PCI_DEVICE(0x11AB, 0xABCD)}, /* FPGA board */
    {PCI_DEVICE(0x11AB, 0x0f13)},
    {PCI_DEVICE(0x11AB, 0x0a72)}, /* Oak */
};

#if CONFIG_PM_SLEEP
/* Device Power Management (DPM) support */
static const struct dev_pm_ops oak_dpm_ops = {
    .suspend = oak_dpm_suspend,
    .resume = oak_dpm_resume,
};
#endif

/* PCIe - interface structure */
static struct pci_driver oak_driver = {
    .name = oak_driver_name,
    .id_table = oak_pci_tbl,
    .probe = oak_probe,
    .remove = oak_remove,
#if CONFIG_PM_SLEEP
    .driver.pm = &oak_dpm_ops,
#endif
};
static struct ethtool_ops oak_ethtool_ops = {
    .get_ethtool_stats = oak_ethtool_get_stats,
    .get_strings = oak_ethtool_get_strings,
    .get_sset_count = oak_ethtool_get_sscnt,
    .get_link = ethtool_op_get_link,
    .get_msglevel = oak_dbg_get_level,
    .get_settings = oak_ethtool_get_link_ksettings,
    .set_msglevel = oak_dbg_set_level,
};
static struct net_device_ops oak_netdev_ops = {
    .ndo_open = oak_net_open,
    .ndo_stop = oak_net_close,
    .ndo_start_xmit = oak_net_xmit_frame,
    .ndo_do_ioctl = oak_net_ioctl,
    .ndo_set_mac_address = oak_net_set_mac_addr,
    /* .ndo_select_queue = oak_net_select_queue, */
    .ndo_change_mtu = oak_net_esu_set_mtu,
};
int debug = 0;
int txs = 1024;
int rxs = 1024;
int chan = MAX_NUM_OF_CHANNELS;
int rto = 100;
int mhdr = 0;
int port_speed = 5;

/* private function prototypes */
static int oak_init_module(void);
static void oak_exit_module(void);
static int oak_probe(struct pci_dev* pdev, const struct pci_device_id* dev_id);
static void oak_remove(struct pci_dev* pdev);

/* Name      : init_module
 * Returns   : int
 * Parameters:
 *  */
static int oak_init_module(void)
{
    /* automatically added for object flow handling */
    int32_t return_3 = 0; /* start of activity code */
    /* UserCode{15A51487-0028-49e6-916F-ED565CFC9C74}:SdSnX58zaJ */
    pr_info("%s - (%s) version %s\n", oak_driver_string, oak_driver_name, oak_driver_version);
    pr_info("%s\n", oak_driver_copyright);

    /* UserCode{15A51487-0028-49e6-916F-ED565CFC9C74} */

    return_3 = pci_register_driver(&oak_driver);
    return return_3;
}

/* Name      : exit_module
 * Returns   : void
 * Parameters:
 *  */
static void oak_exit_module(void)
{
    /* start of activity code */
    pci_unregister_driver(&oak_driver);
    return;
}

/* Name      : probe
 * Returns   : int
 * Parameters:  struct pci_dev * pdev,  const struct pci_device_id * dev_id
 *  */
static int oak_probe(struct pci_dev* pdev, const struct pci_device_id* dev_id)
{
    /* automatically added for object flow handling */
    int return_10; /* start of activity code */
    int err = 0;
    /* UserCode{D4E52465-5E93-4791-A8E0-487EDF3C0F48}:1OdSPm6Zyw */
    oak_t* adapter = NULL;
    /* UserCode{D4E52465-5E93-4791-A8E0-487EDF3C0F48} */

#if CONFIG_PM
    /* Set PCI device power state to D0 */
    err = pci_set_power_state(pdev, PCI_D0);
    if (err == 0)
         pr_info("oak: Device power state D%d\n", pdev->current_state);
    else
         pr_err("oak: Failed to set the device power state err: %d\n", err);
#endif

    err = oak_init_software(pdev);

    if (err == 0)
    {
        /* UserCode{B9F90CBD-AD96-487f-9DC8-ADBE43BFC45B}:1EfdPrOPjH */
        struct net_device* netdev = pci_get_drvdata(pdev);
        adapter = netdev_priv(netdev);
        adapter->level = 10;
        /* UserCode{B9F90CBD-AD96-487f-9DC8-ADBE43BFC45B} */

        err = oak_init_hardware(pdev);

        if (err == 0)
        {
            /* UserCode{B6E06B18-56B2-4fbd-92F4-809A0D84D7CB}:PfOr5t1lbV */
            adapter->level = 20;
            /* UserCode{B6E06B18-56B2-4fbd-92F4-809A0D84D7CB} */
	    sw_phy_int(adapter);

            err = oak_start_hardware();

            if (err == 0)
            {
                /* UserCode{86B109DD-5FE2-4f22-9ABD-7AB17864A0D7}:7O7k1PHroI */
                adapter->level = 30;
                /* UserCode{86B109DD-5FE2-4f22-9ABD-7AB17864A0D7} */

                err = oak_start_software(pdev);

                if (err == 0)
                {
                    /* UserCode{CBBFD955-F8A6-4dd6-AFD9-23D7CA52B0DE}:ZWwjfSm1zy */
                    adapter->level = 40;
                    /* UserCode{CBBFD955-F8A6-4dd6-AFD9-23D7CA52B0DE} */
                }
                else
                {
                    return_10 = err;
                }
            }
            else
            {
                return_10 = err;
            }
        }
        else
        {
            return_10 = err;
        }
    }
    else
    {
        return_10 = err;
    }

    if (err == 0)
    {
        return_10 = err;

        if (adapter->sw_base != NULL)
        {
            /* UserCode{B042BBF4-2A74-4399-9B05-784F95B6EA88}:w7hxjq1dvQ */
            pr_info("%s[%d] - ESU register access is supported", oak_driver_name, pdev->devfn);
            /* UserCode{B042BBF4-2A74-4399-9B05-784F95B6EA88} */
        }
        else
        {
        }
    }
    else
    {
        oak_remove(pdev);
    }
    /* UserCode{68984A9B-81AC-45c2-A14C-F4DDF25E36F2}:mlR93Mg2NF */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p err=%d", pdev, pci_get_drvdata(pdev), err);

    /* UserCode{68984A9B-81AC-45c2-A14C-F4DDF25E36F2} */

    return return_10;
}

/* Name      : remove
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
static void oak_remove(struct pci_dev* pdev)
{
    struct net_device* netdev = pci_get_drvdata(pdev);
    oak_t* adapter = NULL; /* start of activity code */

    if (netdev != NULL)
    {
        /* UserCode{5D226271-6A76-4729-895D-7FF4A47804C9}:4eCpRz3vmY */
        adapter = netdev_priv(netdev);

        /* UserCode{5D226271-6A76-4729-895D-7FF4A47804C9} */
    }
    else
    {
    }

    if (adapter != NULL && adapter->level >= 40)
    {
        oak_stop_software(pdev);
    }
    else
    {
    }

    if (adapter != NULL && adapter->level >= 30)
    {
        oak_stop_hardware();
    }
    else
    {
    }

    if (adapter != NULL && adapter->level >= 20)
    {
        oak_release_hardware(pdev);
    }
    else
    {
    }

    if (adapter != NULL && adapter->level >= 10)
    {
        oak_release_software(pdev);

        if (adapter->sw_base != NULL)
        {
        }
        else
        {
        }
    }
    else
    {
    }
    /* UserCode{CFEEFE3D-B248-4f0d-9D34-9FE578599B3D}:199kbfBgmo */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p", pdev, pci_get_drvdata(pdev));

    /* UserCode{CFEEFE3D-B248-4f0d-9D34-9FE578599B3D} */

    return;
}

/* Name      : get_msix_resources
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_get_msix_resources(struct pci_dev* pdev)
{
    struct net_device* dev = pci_get_drvdata(pdev);
    oak_t* adapter = netdev_priv(dev);
    uint32_t num_irqs = sizeof(adapter->gicu.msi_vec) / sizeof(struct msix_entry);
    uint32_t num_cpus = num_online_cpus();
    int err = 0;
    int i;
    int cnt; /* automatically added for object flow handling */
    int return_1; /* start of activity code */
    /* UserCode{0CF0810D-5FDE-43b9-A05A-C97344F1684E}:khYcdwDcBD */
    cnt = pci_msix_vec_count(pdev);
    /* UserCode{0CF0810D-5FDE-43b9-A05A-C97344F1684E} */

    if (cnt <= 0)
    {
        return_1 = -EFAULT;
    }
    else
    {

        if (cnt <= num_irqs)
        {
            /* UserCode{0C04AADC-779F-4228-B5DF-D87391279646}:Gay4XT2FfA */
            num_irqs = cnt;
            /* UserCode{0C04AADC-779F-4228-B5DF-D87391279646} */
        }
        else
        {
        }

        if (num_irqs > num_cpus)
        {
            /* UserCode{C7F96C12-11BC-4401-9A1B-1D5B34C850D0}:4CYpR7yR8u */
            num_irqs = num_cpus;
            /* UserCode{C7F96C12-11BC-4401-9A1B-1D5B34C850D0} */
        }
        else
        {
        }
        /* UserCode{C3D0D52F-6560-4c47-9ACF-04B62E4773D3}:15kzOHEf3s */
        for (i = 0; i < num_irqs; i++)
        {
            adapter->gicu.msi_vec[i].vector = 0;
            adapter->gicu.msi_vec[i].entry = i;
        }

        /* UserCode{C3D0D52F-6560-4c47-9ACF-04B62E4773D3} */

        /* UserCode{7F39324E-C81E-4e69-B8F5-7D6CC4F56621}:lHSjS0BFZC */
        err = pci_enable_msix_range(pdev, adapter->gicu.msi_vec, num_irqs, num_irqs);

        /* UserCode{7F39324E-C81E-4e69-B8F5-7D6CC4F56621} */

        /* UserCode{58ED1582-7859-469a-BC96-AF9BD90A1895}:Oh2tYNSssE */
        adapter->gicu.num_ldg = num_irqs;
        /* UserCode{58ED1582-7859-469a-BC96-AF9BD90A1895} */

        if (err < 0)
        {
            return_1 = -EFAULT;
        }
        else
        {
            return_1 = 0;
        }
        /* UserCode{E51AF948-8DF9-401e-A088-FCC3A348ECFB}:uZlnwfnaHF */
        oakdbg(debug, PROBE, "pdev=%p ndev=%p num_irqs=%d/%d err=%d", pdev, dev, num_irqs, cnt, err);

        /* UserCode{E51AF948-8DF9-401e-A088-FCC3A348ECFB} */
    }
    return return_1;
}

/* Name      : release_hardware
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_release_hardware(struct pci_dev* pdev)
{
    struct net_device* dev = pci_get_drvdata(pdev);
    oak_t* adapter = netdev_priv(dev);
    int err = 0; /* start of activity code */
    /* UserCode{0E3580D1-501C-4414-9D92-443D07286EDB}:ZzNzE489RP */
    if (adapter->gicu.num_ldg > 0)
    {
        pci_disable_msix(pdev);
    }
    /* UserCode{0E3580D1-501C-4414-9D92-443D07286EDB} */

    /* UserCode{EFF7A608-71E6-469f-8AC3-546A28EDC525}:nM1DlFpdXU */
    pci_release_regions(pdev);
    /* UserCode{EFF7A608-71E6-469f-8AC3-546A28EDC525} */

    /* UserCode{00442176-44C4-422d-9582-6B2EBA4E7CD5}:v0iOY3v2UI */
    pci_disable_device(pdev);
    /* UserCode{00442176-44C4-422d-9582-6B2EBA4E7CD5} */

    /* UserCode{D8F4F7E6-D311-40e6-9180-AF666AD901F1}:mlR93Mg2NF */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p err=%d", pdev, pci_get_drvdata(pdev), err);
    /* UserCode{D8F4F7E6-D311-40e6-9180-AF666AD901F1} */

    return;
}

/* Name      : init_hardware
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_init_hardware(struct pci_dev* pdev)
{
    struct net_device* dev = pci_get_drvdata(pdev);
    oak_t* adapter = netdev_priv(dev);
    int err = 0;
    int mem_flags; /* automatically added for object flow handling */
    int return_5; /* start of activity code */
    /* UserCode{045AE595-45E8-49ac-AEBF-9158A190CECF}:LVMRD7ttvA */
    err = pci_enable_device(pdev);

    /* UserCode{045AE595-45E8-49ac-AEBF-9158A190CECF} */

    /* UserCode{39F4CCEC-4D96-4875-A22A-1E96F77D9DEA}:HVGo34cb8Y */
    mem_flags = pci_resource_flags(pdev, 0);
    /* UserCode{39F4CCEC-4D96-4875-A22A-1E96F77D9DEA} */

    if ((mem_flags & IORESOURCE_MEM) == 0)
    {
        /* UserCode{7714BE33-CC79-4d76-9643-38EB72B02A5E}:2QzXLyzJGK */
        err = -EINVAL;
        /* UserCode{7714BE33-CC79-4d76-9643-38EB72B02A5E} */
    }
    else
    {
        /* UserCode{3761AE1D-25AD-4974-BE1C-0B5737EA5806}:NXNgZ68fgk */
        pci_read_config_dword(pdev, PCI_CLASS_REVISION, &adapter->pci_class_revision);
        adapter->pci_class_revision &= 0x0000000F;

        /* UserCode{3761AE1D-25AD-4974-BE1C-0B5737EA5806} */

        if (adapter->pci_class_revision > OAK_REVISION_B0)
        {
            /* UserCode{82E50DD2-C28C-4bf3-BC64-069A0350892C}:2QzXLyzJGK */
            err = -EINVAL;
            /* UserCode{82E50DD2-C28C-4bf3-BC64-069A0350892C} */
        }
        else
        {

            if (err == 0)
            {
                /* UserCode{503D3DEF-F25D-439a-9CD6-4A392BC30039}:z7opqzJHpF */
                err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
                /* UserCode{503D3DEF-F25D-439a-9CD6-4A392BC30039} */
            }
            else
            {
            }

            if (err != 0)
            {
                /* UserCode{0BDB81D2-5B9E-4800-B220-7D537E087CCB}:bMTduWRIwi */
                err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
                /* UserCode{0BDB81D2-5B9E-4800-B220-7D537E087CCB} */
            }
            else
            {
            }
        }

        if (err == 0)
        {
            /* UserCode{1E3F3E7B-938C-44cf-912A-37404E233B74}:mlkBCiNY1K */
            err = pci_request_regions(pdev, OAK_DRIVER_NAME);
            /* UserCode{1E3F3E7B-938C-44cf-912A-37404E233B74} */
        }
        else
        {
        }

        if (err == 0)
        {
            /* UserCode{EA693964-F421-4759-81B3-F1F8CBF8DC34}:1dzQJzTnK2 */
            pci_set_master(pdev);
            pci_save_state(pdev);

            /* UserCode{EA693964-F421-4759-81B3-F1F8CBF8DC34} */

            if (err == 0)
            {
                /* UserCode{2E4E27E7-C422-454c-BA48-63043E1DB652}:14D5WQFk7O */
                adapter->um_base = pci_iomap(pdev, 0, 0);
                /* UserCode{2E4E27E7-C422-454c-BA48-63043E1DB652} */

                if (adapter->um_base == NULL)
                {
                    /* UserCode{7C3B045A-4118-4ede-ACF9-3F5A59436AE9}:17P14WrLMp */
                    err = -ENOMEM;
                    /* UserCode{7C3B045A-4118-4ede-ACF9-3F5A59436AE9} */
                }
                else
                {
                    /* UserCode{3ED9752B-DD28-4bb4-A21E-FCC00FB3DEE7}:1LawcIBeQE */
                    mem_flags = pci_resource_flags(pdev, 2);
                    /* UserCode{3ED9752B-DD28-4bb4-A21E-FCC00FB3DEE7} */

                    if ((mem_flags & IORESOURCE_MEM) == 0)
                    {
                    }
                    else
                    {
                        /* UserCode{F6A9726E-245C-426a-A735-153B095B5653}:1NUYfnsbbJ */
                        adapter->sw_base = pci_iomap(pdev, 2, 0);
                        /* UserCode{F6A9726E-245C-426a-A735-153B095B5653} */
                    }
                    /* UserCode{85423AA5-9B0F-429b-B507-15BDCD9B81E0}:4Drn053A81 */
                    oakdbg(debug, PROBE, "Device found: dom=%d bus=%d dev=%d fun=%d reg-addr=%p",
                           pci_domain_nr(pdev->bus), pdev->bus->number, PCI_SLOT(pdev->devfn), pdev->devfn, adapter->um_base);

                    /* UserCode{85423AA5-9B0F-429b-B507-15BDCD9B81E0} */

                    /* UserCode{3F93483A-8BD7-4f02-BA01-00C3C9FAD9B0}:cHMFC8bPMZ */
                    {
                        uint32_t v0, v1;

                        pci_read_config_dword(pdev, 0x10, &v0);
                        pci_read_config_dword(pdev, 0x14, &v1);
                        v0 &= 0xfffffff0;
                        v0 |= 1;
                        pci_write_config_dword(pdev, 0x944, v1);
                        pci_write_config_dword(pdev, 0x940, v0);
                    }
                    /* UserCode{3F93483A-8BD7-4f02-BA01-00C3C9FAD9B0} */

                    /* UserCode{6EF87422-F4F9-48f2-8648-D22939E1EE16}:fVLFH4L0DF */
                    {

                        uint16_t devctl;

                        pcie_capability_read_word(pdev, PCI_EXP_DEVCTL, &devctl);

                        adapter->rrs = 1 << (((devctl & PCI_EXP_DEVCTL_READRQ) >> 12) + 6);
                    }
                    /* UserCode{6EF87422-F4F9-48f2-8648-D22939E1EE16} */

                    err = oak_get_msix_resources(pdev);
                }
            }
            else
            {
            }
        }
        else
        {
        }
    }
    return_5 = err;
    /* UserCode{19561B8A-DD8E-4520-8499-12725AF75CDF}:mlR93Mg2NF */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p err=%d", pdev, pci_get_drvdata(pdev), err);
    /* UserCode{19561B8A-DD8E-4520-8499-12725AF75CDF} */

    return return_5;
}

/* Name      : init_pci
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_init_pci(struct pci_dev* pdev)
{
    /* start of activity code */
    return;
}

/* Name      : oak_set_mtu_config
 * Returns   : void
 * Parameters: struct net_device *netdev
 * Description: This function sets the min and max MTU size in the linux netdev.
 */
void oak_set_mtu_config(struct net_device *netdev)
{
    netdev->min_mtu = ETH_MIN_MTU;
    netdev->max_mtu = OAK_MAX_JUMBO_FRAME_SIZE - (ETH_HLEN + ETH_FCS_LEN);
}

/* Name      : init_software
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_init_software(struct pci_dev* pdev)
{
    struct net_device* netdev = NULL;
    oak_t* oak;
    int err = 0; /* automatically added for object flow handling */
    int return_2; /* start of activity code */
    /* UserCode{1A7566C2-C70F-443d-960E-FB23E34E1F9E}:14MzP1MQD0 */
    netdev = alloc_etherdev_mq(sizeof(oak_t), chan);

    /* UserCode{1A7566C2-C70F-443d-960E-FB23E34E1F9E} */

    if (netdev != NULL)
    {
        /* UserCode{B8255E97-3D1D-4c73-9FB2-ED43A46ECE92}:1eGJs9Is5c */
        SET_NETDEV_DEV(netdev, &pdev->dev);
        pci_set_drvdata(pdev, netdev);
        oak = netdev_priv(netdev);
        oak->device = &pdev->dev;
        oak->netdev = netdev;
        oak->pdev = pdev;
        /* UserCode{B8255E97-3D1D-4c73-9FB2-ED43A46ECE92} */

#if CONFIG_PM
       /* Create sysfs entry for D0, D1, D2 and D3 states testing */
       oak_dpm_create_sysfs(oak);
#endif

        return_2 = err;

        if (err != 0)
        {
            oak_release_software(pdev);
        }
        else
        {
            /* UserCode{38D79BE3-9956-4d88-8279-6757E485A1FE}:tesoi2C7cs */
            netdev->netdev_ops = &oak_netdev_ops;
            netdev->features = oak_chksum_get_config();
            oak_set_mtu_config(netdev);
            spin_lock_init(&oak->lock);
            /* UserCode{38D79BE3-9956-4d88-8279-6757E485A1FE} */
        }
    }
    else
    {
        return_2 = err = -ENOMEM;
    }
    /* UserCode{5AF52ED9-43FE-475f-84BF-964C2C4928A9}:mlR93Mg2NF */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p err=%d", pdev, pci_get_drvdata(pdev), err);
    /* UserCode{5AF52ED9-43FE-475f-84BF-964C2C4928A9} */

    return return_2;
}

/* Name      : release_software
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_release_software(struct pci_dev* pdev)
{
    int retval;
    struct net_device* netdev = pci_get_drvdata(pdev); /* start of activity code */
    oak_t* oak;
    oak = netdev_priv(netdev);

    /* UserCode{2F73D0CB-CB50-40fe-B322-F30B1A2AB2B1}:199kbfBgmo */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p", pdev, pci_get_drvdata(pdev));
    /* UserCode{2F73D0CB-CB50-40fe-B322-F30B1A2AB2B1} */

#if CONFIG_PM
    /* Set the PCI device power state to D3hot */
    retval = pci_set_power_state(pdev, PCI_D3hot);
    if (retval == 0)
        pr_info("oak: Device power state D%d\n", pdev->current_state);
    else
         pr_err("oak: Failed to set the device power state err: %d\n", retval);

    /* Remove sysfs entry */
    oak_dpm_remove_sysfs(oak);
#endif

    /* UserCode{87B990A3-874F-4ad1-AC84-DD968E06EF5F}:11nvw95fpZ */
    free_netdev(netdev);
    /* UserCode{87B990A3-874F-4ad1-AC84-DD968E06EF5F} */

    return;
}

/* Name      : start_hardware
 * Returns   : int
 * Parameters:
 *  */
int oak_start_hardware(void)
{
    /* automatically added for object flow handling */
    int return_1; /* start of activity code */
    return_1 = 0;
    return return_1;
}

/* Name      : start_software
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_start_software(struct pci_dev* pdev)
{
    struct net_device* netdev = pci_get_drvdata(pdev);
    int err = 0; /* automatically added for object flow handling */
    int return_2; /* start of activity code */
    /* UserCode{87C65DAE-FE7F-431d-A228-F340FBA3ED6E}:uyU18eMDt5 */
    netdev->ethtool_ops = &oak_ethtool_ops;
    /* UserCode{87C65DAE-FE7F-431d-A228-F340FBA3ED6E} */

    oak_net_add_napi(netdev);
    /* UserCode{B9650390-79FB-4f72-BC9A-1DC5BF52FD2E}:ejE8ZxF32E */
    err = register_netdev(netdev);
    /* UserCode{B9650390-79FB-4f72-BC9A-1DC5BF52FD2E} */
    {
	struct sockaddr addr;
	eth_random_addr(addr.sa_data);
	oak_net_set_mac_addr(netdev, &addr);
    }

    return_2 = 0;
    return return_2;
}

/* Name      : stop_hardware
 * Returns   : void
 * Parameters:
 *  */
void oak_stop_hardware(void)
{
    /* start of activity code */
    return;
}

/* Name      : stop_software
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_stop_software(struct pci_dev* pdev)
{
    struct net_device* netdev = pci_get_drvdata(pdev); /* start of activity code */
    /* UserCode{4605CD77-3541-4856-BC73-81E5F109AD07}:XKWNJ00x3g */
    unregister_netdev(netdev);
    /* UserCode{4605CD77-3541-4856-BC73-81E5F109AD07} */

    return;
}
