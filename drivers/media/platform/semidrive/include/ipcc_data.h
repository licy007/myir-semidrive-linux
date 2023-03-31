/*
 * Copyright (c) 2022 Semidrive Semiconductor, Inc.
 * All rights reserved.
 */

#ifndef __IPCC_DATA_H__
#define __IPCC_DATA_H__

#include <linux/types.h>

#define VPU_IPCC_DATA_LEN_MAX 468

#define VPU_IPC_RETURN_INVAILD 0x00
#define VPU_IPC_RETUERN_OK 0x01
#define VPU_IPC_RETURN_ERROR 0xff

#define VPU_VIRTIO_TIMEOUT 1200 //greater than the waiting time of the vpu interrupt

typedef enum {
	VPU_IPC_REQUEST_LOCK,
	VPU_IPC_RELEASE_LOCK,
	VPU_IPC_HEARTBEAT,
	VPU_IPC_CLEAR_INSTANCE,
} vpu_ipc_cmd_t;

typedef enum {
	VPU_DEVICES_CODA988,
	VPU_DEVICES_WAVE412,
	VPU_DEVICES_MAX,
} vpu_devices_t;

typedef struct vpu_ipcc_data {
	uint8_t cmd;
	uint8_t ipcc_id;
	uint8_t dev_id;
	uint8_t return_value;
	uint8_t cmd_param[VPU_IPCC_DATA_LEN_MAX];
	int32_t cmd_param_full_length;
	int32_t current_packed_start_offset;
	int32_t current_packed_end_offset;
	int32_t reserve0;
	int32_t reserve1;
} __attribute__((packed)) vpu_ipcc_data_t;

#endif /* __IPCC_DATA_H__ */
