/*
 * Copyright (C) 2020 Semidriver Semiconductor
 * License terms:  GNU General Public License (GPL), version 2
 */

#ifndef __SDRV_PROPERTY_H__
#define __SDRV_PROPERTY_H__

struct sys_property_value {
	char *val;
	char *name;
	int id;
};

#define PROP_NAME_MAX   32
#define PROP_VALUE_MAX  92

struct sdrv_prop_ioctl {
	int id;
	char name[PROP_NAME_MAX];
	char val[PROP_VALUE_MAX];
};

#define SDRV_PROP_IOCTL_BASE			 'p'

#define SDRV_PROP_IO(nr)			_IO(SDRV_PROP_IOCTL_BASE, nr)
#define SDRV_PROP_IOR(nr, type)		_IOR(SDRV_PROP_IOCTL_BASE, nr, type)
#define SDRV_PROP_IOW(nr, type)		_IOW(SDRV_PROP_IOCTL_BASE, nr, type)
#define SDRV_PROP_IOWR(nr, type)	_IOWR(SDRV_PROP_IOCTL_BASE, nr, type)

#define SDRV_PROP_ADD_CMD_CODE		1
#define SDRV_PROP_GET_CMD_CODE		2
#define SDRV_PROP_FOREACH_CMD_CODE	3

#define PCMD_SDRV_PROP_ADD \
	SDRV_PROP_IOWR(SDRV_PROP_ADD_CMD_CODE, struct sdrv_prop_ioctl)
#define PCMD_SDRV_PROP_GET \
	SDRV_PROP_IOWR(SDRV_PROP_GET_CMD_CODE, struct sdrv_prop_ioctl)
#define PCMD_SDRV_PROP_FOREACH \
	SDRV_PROP_IOWR(SDRV_PROP_FOREACH_CMD_CODE, struct sdrv_prop_ioctl)

void sdrv_property_list_init(void);
int sdrv_property_list_set_val(int property_id, void *val);
int sdrv_property_list_get_val(int property_id, void *val);
int sdrv_property_str2list(int flag, char *str, int len);
int sdrv_property_show(void);
int sdrv_property_list_getlen(void);
int sdrv_property_client_set_val(int property_id, void *name, void *val);
int sdrv_property_list_find(char *name, char *val, int len);
int sdrv_property_list_get_name_val(int property_id,
	char *name, int nlen, char *val, int vlen);
int prop_client_send_set_val(char *update_msg, int msg_len);

#endif /* __SDRV_PROPERTY_H__ */

