#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mm.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/memory.h>
#include <asm/tlbflush.h>
#include <asm/fixmap.h>
#include <asm/pgalloc.h>
#include <linux/slab.h>


#define ADI_ADDR_SIZE	0x10000
#define ADI_OFFSET	0x8000

#define LOOKAT_RWPV			(1<<0)
#define LOOKAT_INPUT_OUTPUT_DATA	(1<<1)
#define __READ__			(0<<1)
#define __WRITE__			(1<<1)
#define __P__				(0<<0)
#define __V__				(1<<0)

struct smd_lookat {
	const char *name;
	unsigned int entry_type;
};
static struct smd_lookat __lookat_array[] = {
	{"addr_rwpv", LOOKAT_RWPV},
	{"data", LOOKAT_INPUT_OUTPUT_DATA},
};

struct lookat_request {
	unsigned int cmd;
	unsigned long addr;
	unsigned long value;

	struct list_head list;
};

struct lookat {
	struct mutex list_lock;
	struct regmap *regmap;
	struct lookat_request request;
	struct list_head request_list;
};

static struct lookat lookat_desc;

static int smd_is_adi_vaddr(unsigned long vaddr)
{
	return 0;
}

static int smd_adi_p2v(unsigned long paddr, unsigned long *vaddr)
{
		return -EINVAL;
}

static inline void chip_raw_ddie_write(unsigned long vreg,
				       unsigned long or_val,
				       unsigned int clear_msk)
{
	unsigned long val = readl_relaxed((void *)vreg);

	writel_relaxed((val & ~clear_msk) | or_val, (void *)vreg);
}

static int smd_read_va(unsigned long vreg, unsigned long *val)
{
	if (!val)
		return -EINVAL;

	if (unlikely(smd_is_adi_vaddr(vreg)))
		regmap_read(lookat_desc.regmap, (unsigned int)vreg,
			    (unsigned int *)val);
	else
		*val = readl_relaxed((void __iomem *)vreg);

	return 0;
}

static int smd_write_va(unsigned long vreg, const unsigned long or_val,
			 const unsigned int clear_msk)
{
	if (unlikely(smd_is_adi_vaddr(vreg)))
		regmap_write(lookat_desc.regmap, (unsigned int)vreg,
			     (unsigned int)or_val);
	else
		chip_raw_ddie_write(vreg, or_val, clear_msk);

	return 0;
}

static int smd_read_pa(unsigned long preg, unsigned long *val)
{
	void *addr;

	if (!val)
		return -EINVAL;

	addr = __va(preg);
	pr_debug("%s 0x%lx,0x%lx,%d\n", __func__, preg, (unsigned long)addr,
		 virt_addr_valid(addr));
	if (!virt_addr_valid(addr)) {
		void __iomem *io_addr;
		unsigned long vaddr = 0;

		if (!smd_adi_p2v(preg, &vaddr))
			regmap_read(lookat_desc.regmap, (unsigned int)vaddr,
				    (unsigned int *)val);
		else {
			io_addr = ioremap_nocache(preg, PAGE_SIZE);
			if (!io_addr) {
				pr_warn("unable to map i/o region\n");
				return -ENOMEM;
			}
			*val = readl_relaxed((void __iomem *)io_addr);
			iounmap(io_addr);
		}
	} else
		*val = *(unsigned long *)addr;

	return 0;
}

static int smd_write_pa(unsigned long paddr, const unsigned long or_val,
			 const unsigned int clear_msk)
{
	void *addr;
	unsigned long val;
	pr_debug("%s 0x%lx, 0x%lx, 0x%x\n", __func__, paddr, or_val, clear_msk);
	addr = __va(paddr);
	if (!virt_addr_valid(addr)) {
		void __iomem *io_addr;
		unsigned long vaddr = 0;

		if (!smd_adi_p2v(paddr, &vaddr))
			regmap_write(lookat_desc.regmap, (unsigned int)vaddr,
				     (unsigned int)or_val);
		else {
			io_addr = ioremap_nocache(paddr, PAGE_SIZE);
			if (!io_addr) {
				pr_warn("unable to map i/o region\n");
				return -ENOMEM;
			}
			chip_raw_ddie_write((unsigned long)io_addr, or_val,
					    clear_msk);
			iounmap(io_addr);
		}
	} else{
		val = *(unsigned long *) addr;
		*(unsigned long *) addr = ((val & ~clear_msk) | or_val);

	}

	return 0;
}

static int queue_add_element(unsigned long raw_cmd)
{
	struct lookat_request *request;
	request = kzalloc(sizeof(struct lookat_request), GFP_KERNEL);
	if (!request)
		return -ENOMEM;

	request->cmd = raw_cmd & (0x3);
	request->addr = raw_cmd & (~0x3);

	mutex_lock(&lookat_desc.list_lock);
	list_add_tail(&request->list, &lookat_desc.request_list);
	mutex_unlock(&lookat_desc.list_lock);

	return 0;
}

static struct lookat_request *return_last_eliment(void)
{
	struct lookat_request *entry;

	mutex_lock(&lookat_desc.list_lock);
	list_for_each_entry(entry, &lookat_desc.request_list, list) {
		lookat_desc.request.addr  = entry->addr;
		lookat_desc.request.cmd   = entry->cmd;
		lookat_desc.request.value  = entry->value;
		list_del(&entry->list);
		break;
	}
	mutex_unlock(&lookat_desc.list_lock);

	return entry;
}

static inline int queue_modify_value(unsigned long value)
{
	if (!(lookat_desc.request.cmd & __WRITE__))
		return 0;

	lookat_desc.request.value = value;

	return 1;
}

static int do_request(void)
{
	int ret = 0;

	if ((lookat_desc.request.cmd & __WRITE__) == __WRITE__) {
		if ((lookat_desc.request.cmd & __V__) == __V__)
			ret =
			    smd_write_va(lookat_desc.request.addr,
					  lookat_desc.request.value, ~0);
		else
			ret =
			    smd_write_pa(lookat_desc.request.addr,
					  lookat_desc.request.value, ~0);
	} else {
		if ((lookat_desc.request.cmd & __V__) == __V__)
			ret =
			    smd_read_va(lookat_desc.request.addr,
					 &lookat_desc.request.value);
		else
			ret =
			    smd_read_pa(lookat_desc.request.addr,
					 &lookat_desc.request.value);
	}

	return ret;
}

static int debug_set(void *data, u64 val)
{
	struct smd_lookat *p = data;
	int ret = 0;
	struct lookat_request *r;

	if (p->entry_type == LOOKAT_INPUT_OUTPUT_DATA) {
		if (queue_modify_value((unsigned long)val))
			ret = do_request();
	} else if (p->entry_type == LOOKAT_RWPV) {
		ret = queue_add_element((unsigned long)val);
		if (ret) {
			pr_err("%s, no memory add element\n", __func__);
			return -1;
		}

		r = return_last_eliment();
		if (!r) {
			pr_err("%s,return last eliment error\n", __func__);
			return -1;
		}

		if (!((r->cmd & __WRITE__) == __WRITE__))
			ret = do_request();
		kfree(r);
	} else {
		pr_err("Please entry the available entry type\n");
	}

	return ret;
}

static int debug_get(void *data, u64 *val)
{
	struct smd_lookat *p = data;

	if (p->entry_type == LOOKAT_INPUT_OUTPUT_DATA) {
		*val = lookat_desc.request.value;
	} else if (p->entry_type == LOOKAT_RWPV) {
		pr_info("echo addr | b00 to read Paddr's value\n");
		pr_info("echo addr | b01 to read Vaddr's value\n");
		pr_info
		    ("echo addr | b10 to write Paddr's value,Pls echo value to data file\n");
		pr_info
		    ("echo addr | b11 to write Vaddr's value,Pls echo value to data file\n");
	} else
		return -EINVAL;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(lookat_fops, debug_get, debug_set, "0x%08llx");

static int __init debug_add(struct smd_lookat *lookat, struct dentry *parent)
{
	if (!debugfs_create_file(lookat->name, 0644, parent,
				 lookat, &lookat_fops))
		return -ENOENT;

	return 0;
}

static int __init lookat_debug_init(void)
{
	int i;
	struct dentry *lookat_base;

	lookat_base = debugfs_create_dir("lookat", NULL);
	if (!lookat_base)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(__lookat_array); ++i) {
		if (debug_add(&__lookat_array[i], lookat_base)) {
			debugfs_remove(lookat_base);
			return -ENOENT;
		}
	}

	mutex_init(&lookat_desc.list_lock);
	INIT_LIST_HEAD(&lookat_desc.request_list);

	pr_info("%s init ok\n", __func__);

	return 0;
}

module_init(lookat_debug_init);
MODULE_AUTHOR("tao");
