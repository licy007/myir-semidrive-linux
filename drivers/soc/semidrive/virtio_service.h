#pragma once

#include <linux/device.h>
#include <linux/types.h>
#include <linux/virtio.h>


#define MMIO_REG_SIZE				0x100
#define MMIO_CFG_SIZE				0x100
#define VRING_SIZE				0x1b00
#define VIRTIO_FRONTEND_MAX_NUM			33
#define VIRTIO_TOTAL_SIZE			0x40000

/* DO NOT CHANGE */
#define PROVIDER_TX_VRING_LEN			16u
#define PROVIDER_RX_VRING_LEN			16u
#define PROVIDER_TX_BUFSIZE			0x200
#define PROVIDER_RX_BUFSIZE			0x200

/* size is 0x4200, layout from start of share memory */
#define VIRTIO_MMIO_TOTAL_SIZE			((MMIO_REG_SIZE+MMIO_CFG_SIZE)*VIRTIO_FRONTEND_MAX_NUM)

/* size is 0x4000, layout right after mmio area */
#define VIRTIO_PROVIDER_BUFSIZE			(PROVIDER_TX_VRING_LEN*PROVIDER_TX_BUFSIZE+PROVIDER_RX_VRING_LEN*PROVIDER_RX_BUFSIZE)

/* size is 0x37b00, layout right after provider buffers */
#define VIRTIO_VRING_TOTAL_SIZE			(VRING_SIZE*VIRTIO_FRONTEND_MAX_NUM)

/* size is 0x300, layout right after vring area */
#define VIRTIO_UNUSED_SIZE			(0x300)

/* compile-time check */
#if ((VIRTIO_MMIO_TOTAL_SIZE+VIRTIO_PROVIDER_BUFSIZE+VIRTIO_VRING_TOTAL_SIZE+VIRTIO_UNUSED_SIZE) != VIRTIO_TOTAL_SIZE)
virtio_memory_layout_is_incorrect;
#endif

size_t virtio_service_get_provider_memory(
		struct device *dev, void **va, phys_addr_t *pa);

void virtio_service_on_device_change(struct device *dev, u32 vid, u32 addr);

size_t virtio_provider_send_text(
		struct virtio_device *vdev, const char *buf, size_t len);

void virtio_provider_latency_test(struct virtio_device *vdev, u32 count);
