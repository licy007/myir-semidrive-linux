/*
* vmm_type_xen.h
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: System Configuration functions.
*
* Revision History:
* -----------------
* 011, 11/10/2020 Lili create this file
*/

#ifndef VMM_TYPE_XEN_H
#define VMM_TYPE_XEN_H

void XenHostDestroyConnection(IMG_UINT32 uiOSID, IMG_UINT32 uiDevID);
PVRSRV_ERROR XenHostCreateConnection(IMG_UINT32 uiOSID, IMG_UINT32 uiDevID);
#endif
