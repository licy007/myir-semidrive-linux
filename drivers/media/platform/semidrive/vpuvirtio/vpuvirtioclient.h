/*
 * Copyright (c) 2022 Semidrive Semiconductor, Inc.
 * All rights reserved.
 */

#ifndef __VPU_VIRTIO_CLIENT_H__
#define __VPU_VIRTIO_CLIENT_H__

#include "ipcc_data.h"

int vpu_virtio_send_sync(uint32_t dev_id, vpu_ipcc_data_t *msg);
int vpu_virtio_send_async(uint32_t dev_id, vpu_ipcc_data_t *msg);

int vpu_virtio_send_headbeat_pack(uint32_t dev_id);
int vpu_virtio_send_clear_instance(uint32_t dev_id, uint32_t instance);

#endif /* __VPU_VIRTIO_CLIENT_H__ */