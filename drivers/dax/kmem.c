// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2016-2018 Intel Corporation. All rights reserved. */
#include <linux/memremap.h>
#include <linux/pagemap.h>
#include <linux/memory.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pfn_t.h>
#include <linux/slab.h>
#include <linux/dax.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include "dax-private.h"
#include "bus.h"

int dev_dax_kmem_probe(struct device *dev)
{
	struct dev_dax *dev_dax = to_dev_dax(dev);
	struct resource *res = &dev_dax->region->res;
	resource_size_t kmem_start;
	resource_size_t kmem_size;
	struct resource *new_res;
	int numa_node;
	int rc;

	/* Hotplug starting at the beginning of the next block: */
	kmem_start = ALIGN(res->start, memory_block_size_bytes());

	kmem_size = resource_size(res);
	/* Adjust the size down to compensate for moving up kmem_start: */
        kmem_size -= kmem_start - res->start;
	/* Align the size down to cover only complete blocks: */
	kmem_size &= ~(memory_block_size_bytes() - 1);

	new_res = devm_request_mem_region(dev, kmem_start, kmem_size,
					  dev_name(dev));

	if (!new_res) {
		printk("could not reserve region %016llx -> %016llx\n",
				kmem_start, kmem_start+kmem_size);
		return -EBUSY;
	}

	/*
	 * Set flags appropriate for System RAM.  Leave ..._BUSY clear
	 * so that add_memory() can add a child resource.
	 */
	new_res->flags = IORESOURCE_SYSTEM_RAM;
	new_res->name = dev_name(dev);

	numa_node = dev_dax->target_node;
	if (numa_node < 0) {
		pr_warn_once("bad numa_node: %d, forcing to 0\n", numa_node);
		numa_node = 0;
	}

	rc = add_memory(numa_node, new_res->start, resource_size(new_res));
	if (rc)
		return rc;

	return 0;
}
EXPORT_SYMBOL_GPL(dev_dax_kmem_probe);

static int dev_dax_kmem_remove(struct device *dev)
{
	/* Assume that hot-remove will fail for now */
	return -EBUSY;
}

static struct dax_device_driver device_dax_kmem_driver = {
	.drv = {
		.probe = dev_dax_kmem_probe,
		.remove = dev_dax_kmem_remove,
	},
};

static int __init dax_kmem_init(void)
{
	return dax_driver_register(&device_dax_kmem_driver);
}

static void __exit dax_kmem_exit(void)
{
	dax_driver_unregister(&device_dax_kmem_driver);
}

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL v2");
module_init(dax_kmem_init);
module_exit(dax_kmem_exit);
MODULE_ALIAS_DAX_DEVICE(0);
