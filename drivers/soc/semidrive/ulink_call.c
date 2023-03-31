/*
 * Copyright (c) 2021, Semidriver Semiconductor
 *
 * This program is free software; you can rediulinkibute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is diulinkibuted in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/io.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ulink.h>
#include <linux/workqueue.h>

struct ulink_call *ulink_request_call(uint32_t rproc, uint32_t addr_sub)
{
	struct ulink_call *call;

	call = kzalloc(sizeof(*call), GFP_KERNEL);
	if (!call)
		goto err;

	call->ch = ulink_request_channel(rproc, addr_sub, NULL);
	if (!call->ch)
		goto err_channel;

	mutex_init(&call->call_mutex);
	return call;

err_channel:
	kfree(call);
err:
	return NULL;
}
EXPORT_SYMBOL(ulink_request_call);

void ulink_release_call(struct ulink_call *call)
{

	if (call->ch)
		ulink_release_channel(call->ch);
	kfree(call);
}
EXPORT_SYMBOL(ulink_release_call);

uint32_t ulink_call(struct ulink_call *call, struct ulink_call_msg *smsg, struct ulink_call_msg *rmsg)
{
	uint8_t *temp;
	uint32_t resize;

	if (!call->ch)
		return -ENODEV;

	if (unlikely(smsg->size > CALL_PARA_MAX))
		resize = CALL_PARA_MAX;
	else
		resize = smsg->size;

	mutex_lock(&call->call_mutex);

	if (ulink_channel_send(call->ch, smsg->data, resize))
		return -EBUSY;
	resize = ulink_channel_rev(call->ch, &temp);

	if (unlikely(resize > CALL_PARA_MAX))
		resize = CALL_PARA_MAX;

	memcpy(rmsg->data, temp, resize);
	rmsg->size = resize;

	mutex_unlock(&call->call_mutex);

	return 0;
}
EXPORT_SYMBOL(ulink_call);

MODULE_DESCRIPTION("ulink test driver");
MODULE_AUTHOR("mingmin.ling <mingmin.ling@semidrive.com");
MODULE_LICENSE("GPL v2");
