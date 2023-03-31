#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "sdrv_property.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define NULL_NAME " "
#define ZERO_VAL "0"

static struct sys_property_value *g_sdrv_prop;
static int g_sdrv_prop_count;
struct mutex g_list_lock;
static const char prop_sparator[] = "\n";

enum str_flag {
	STR_NAME = 0,
	STR_NAME_VAL = 1,
	STR_VAL = 2,
	STR_MAX,
};

// extern int prop_client_send_set_val(char *update_msg, int msg_len);

// ==== copy from libc/strtok.c ====
static char *strtok_r(char *s, const char *delim, char **last)
{
	const char *spanp;
	int c, sc;
	char *tok;

	if (s == NULL) {
		s = *last;
		if (s == NULL)
			return NULL;
	}

	/*
	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 */
cont:
	c = *s++;
	for (spanp = delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return NULL;
	}
	tok = s - 1;

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (;;) {
		c = *s++;
		spanp = delim;
		do {
			sc = *spanp++;
			if (sc == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = '\0';
				*last = s;
				return tok;
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

static char *strtok(char *s, const char *delim)
{
	static char *last;

	return strtok_r(s, delim, &last);
}
// ==== copy from libc/strtok.c ====

int sdrv_property_item_check(struct sys_property_value *property_table, int num)
{
	// printf("1 [%d][%s][%s]\n",
	//	property_table->id,
	//	property_table->name,
	//	property_table->val);

	struct sys_property_value *item = NULL;
	int i = 0;

	for (i = 0; i < num; i++) {
		if (num == 1)
			item = property_table;
		else
			item = &(property_table[i]);

		if (item->id == 0)
			item->id = g_sdrv_prop_count;

		if (item->name == NULL) {
			// printf("NULL_NAME: %d\n", sizeof(NULL_NAME));
			item->name = kmalloc((sizeof(NULL_NAME)), GFP_KERNEL);
			memset(item->name, 0x00, sizeof(NULL_NAME));
			memcpy(item->name, NULL_NAME, strlen(NULL_NAME));
		}

		if (item->val == NULL) {
			// printf("ZERO_VAL: %d\n", strlen(ZERO_VAL));
			item->val = kmalloc((sizeof(ZERO_VAL)), GFP_KERNEL);
			memset(item->val, 0x00, sizeof(ZERO_VAL));
			memcpy(item->val, ZERO_VAL, strlen(ZERO_VAL));
		}
	}

	// printf("2 [%d][%s][%s]\n",
	//	property_table->id,
	//	property_table->name,
	//	property_table->val);

	return 0;
}

// list维护的函数
// 部分追踪时，应预留id的位置
int sdrv_property_list_add(struct sys_property_value *property_table, int num)
{
	int ret = 0;
	struct sys_property_value *property_area = NULL;
	int num_bak = 0;

	mutex_lock(&g_list_lock);
	sdrv_property_item_check(property_table, num);
	// pr_err("list add [%d] name[%s] val[%s]\n",
	//	property_table->id,
	//	property_table->name,
	//	property_table->val);

	if (property_table && num) {
		// add
		if (g_sdrv_prop && g_sdrv_prop_count) {
			num_bak = g_sdrv_prop_count;
			g_sdrv_prop_count += num;

			property_area = kmalloc_array(g_sdrv_prop_count,
				sizeof(struct sys_property_value), GFP_KERNEL);
			if (!property_area) {
				// pr_err("add not enough
				//	memory for properties %d\n",
				//	g_sdrv_prop_count);
				ret = -1;
				goto fail_and_free;
			}

			memcpy(property_area,
				g_sdrv_prop,
				sizeof(struct sys_property_value) * num_bak);
			memcpy(property_area + num_bak,
				property_table,
				sizeof(struct sys_property_value) * num);
			// 如果频繁的malloc/free会产生内存碎片化
			kfree(g_sdrv_prop);
			g_sdrv_prop = property_area;
		}
		// init
		else {
			property_area = kmalloc_array(num,
				sizeof(struct sys_property_value),
					GFP_KERNEL);
			if (!property_area) {
				// pr_err("init not enough
				// memory for properties %d\n", num);
				ret = -1;
				goto fail_and_free;
			}

			memcpy(property_area,
				property_table,
				sizeof(struct sys_property_value) * num);
			g_sdrv_prop = property_area;
			g_sdrv_prop_count = num;
		}
	}

fail_and_free:
	mutex_unlock(&g_list_lock);
	return ret;
}

int sdrv_property_list_getlen(void)
{
	return g_sdrv_prop_count;
}

int sdrv_property_client_set_val(int property_id, void *name, void *val)
{
	char *update_msg = NULL;
	int update_msg_len = 0;
	int cpoy_idx = 0;
	int *prop_id = NULL;

	if (val == NULL)
		return -1;
	if (property_id >= g_sdrv_prop_count)
		return -1;

	update_msg_len = sizeof(int) +
		PROP_VALUE_MAX + PROP_NAME_MAX + strlen(prop_sparator);
	update_msg = kmalloc(update_msg_len, GFP_KERNEL);
	memset(update_msg, 0x00, update_msg_len);

	prop_id = (int *)update_msg;
	*prop_id = property_id;
	cpoy_idx = sizeof(int);

	if (name != NULL) {
		memcpy(update_msg + cpoy_idx, name, strlen(name));
		cpoy_idx += strlen(name);
	}

	memcpy(update_msg + cpoy_idx, prop_sparator, strlen(prop_sparator));
	cpoy_idx += strlen(prop_sparator);

	memcpy(update_msg + cpoy_idx, val, strlen(val));
	cpoy_idx += strlen(val);

	prop_client_send_set_val(update_msg, cpoy_idx);

	kfree(update_msg);

	return 0;
}

int sdrv_property_list_set_val(int property_id, void *val)
{
	char *csave = NULL;
	struct sys_property_value *property = NULL;

	if (val == NULL)
		return -1;

	if (property_id < 0 || property_id >= g_sdrv_prop_count)
		return -1;

	if (g_sdrv_prop == NULL)
		return -1;

	mutex_lock(&g_list_lock);

	property = &g_sdrv_prop[property_id];

	csave = (char *)val;
	if (property->val != NULL) {
		pr_info("old [%d] val[%s]\n", property_id, property->val);
		kfree(property->val);
		property->val = NULL;
	}
	property->val = kmalloc(strlen(csave) + 1, GFP_KERNEL);
	memset(property->val, 0x00, strlen(csave) + 1);
	memcpy(property->val, csave, strlen(csave));
	pr_info("set [%d] val[%s]\n", property_id, property->val);

	mutex_unlock(&g_list_lock);

	return 0;
}

int sdrv_property_list_get_val(int property_id, void *val)
{
	struct sys_property_value *property = NULL;
	char *cret = NULL;

	if (val == NULL)
		return -1;

	if (property_id < 0 || property_id >= g_sdrv_prop_count)
		return -1;

	if (g_sdrv_prop == NULL)
		return -1;

	mutex_lock(&g_list_lock);

	property = &g_sdrv_prop[property_id];

	cret = (char *)val;
	memcpy(cret, property->val, strlen(property->val));
	// pr_info("return char val[%s]\n", cret);

	mutex_unlock(&g_list_lock);

	return 0;
}

int sdrv_property_list_get_name_val(int property_id,
	char *name, int nlen, char *val, int vlen)
{
	struct sys_property_value *property = NULL;

	if (name == NULL || val == NULL)
		return -1;

	if (property_id < 0 || property_id >= g_sdrv_prop_count)
		return -1;

	if (g_sdrv_prop == NULL)
		return -1;

	mutex_lock(&g_list_lock);

	property = &g_sdrv_prop[property_id];
	memcpy(name, property->name, MIN(strlen(property->name), nlen));
	memcpy(val, property->val, MIN(strlen(property->val), vlen));

	mutex_unlock(&g_list_lock);

	return 0;
}

int sdrv_property_list_find(char *name, char *val, int len)
{
	int ret_id = -1;
	int i = 0;

	if (name == NULL || val == NULL)
		return ret_id;

	mutex_lock(&g_list_lock);
	for (i = 0; i < g_sdrv_prop_count; i++) {
		if (strcmp(g_sdrv_prop[i].name, name) == 0) {
			ret_id = i;
			memcpy(val,
				g_sdrv_prop[i].val,
				MIN(len, strlen(g_sdrv_prop[i].val)));
			break;
		}
	}
	mutex_unlock(&g_list_lock);

	return ret_id;
}

// name
// name + val
int sdrv_property_str2list(int flag, char *str, int len)
{
	char *name = NULL;
	char *val = NULL;
	const char d[2] = "\n";
	struct sys_property_value *tmp_prop = NULL;
	char *token = NULL;

	// mutex_lock(&g_list_lock);
	// name or name + val
	if (flag != STR_VAL) {
		token = strtok(str, d);
		while (token != NULL) {
			// name
			name = kmalloc(strlen(token) + 1, GFP_KERNEL);
			memset(name, 0x00, strlen(token) + 1);
			memcpy(name, token, strlen(token));

			// val
			if (flag == STR_NAME_VAL) {
				token = strtok(NULL, d);
				if (token != NULL) {
					val = kmalloc(strlen(token) + 1,
						GFP_KERNEL);
					memset(val, 0x00, strlen(token) + 1);
					memcpy(val, token, strlen(token));
				} else {
					pr_err(
					"can not find \\n, error line\n"
					);
					break;
				}
			} else {
				val = kmalloc((sizeof(ZERO_VAL)),
					GFP_KERNEL);
				memset(val, 0x00, sizeof(ZERO_VAL));
				memcpy(val, ZERO_VAL, strlen(ZERO_VAL));
			}

			tmp_prop = kmalloc(sizeof(struct sys_property_value),
				GFP_KERNEL);
			memset(tmp_prop,
				0x00,
				sizeof(struct sys_property_value));

			tmp_prop->name = name;
			tmp_prop->val = val;
			tmp_prop->id = g_sdrv_prop_count;

			// pr_err("list[%d] name[%s] val[%s]\n",
			//	tmp_prop->id,
			//	tmp_prop->name,
			//	tmp_prop->val);
			sdrv_property_list_add(tmp_prop, 1);
			// prop已经拷贝到数组中保存，这里释放掉
			// name和val不能释放，数组中需要使用，
			// 由数组deinit的时候释放
			kfree(tmp_prop);
			token = strtok(NULL, d);
		}
	}

	// mutex_unlock(&g_list_lock);
	return 0;
}

void sdrv_property_list_init(void)
{
	mutex_init(&g_list_lock);
}

int sdrv_property_show(void)
{
	int i = 0;

	for (i = 0; i < g_sdrv_prop_count; i++) {
		pr_err("[%d][%s][%s]\n",
			g_sdrv_prop[i].id,
			g_sdrv_prop[i].name,
			g_sdrv_prop[i].val);
	}

	return 0;
}
