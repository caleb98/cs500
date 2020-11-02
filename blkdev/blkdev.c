#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkdev.h>	// for block_device_operations struct
#include <linux/genhd.h>	// for gendisk struct
#include <linux/types.h>
#include <linux/slab.h>

/*
 * Provide module metadata
 */
MODULE_AUTHOR("Caleb Cassady <caleb.cassady@hotmail.com>");
MODULE_DESCRIPTION("A practice block device module");
MODULE_LICENSE("GPL");

#define DEV_NAME "myblkdev"

static int myblkdev_major = 0;
module_param(myblkdev_major, int, 0);
static int dev_sector_size = 512;
module_param(dev_sector_size, int 512);
static int num_sectors = 1024;
module_param(num_sectors, int 1024);
static int num_devices = 4;
module_param(num_devices, int, 4);

struct myblkdev {
	int size;						//Device size in sectors
	u8 *data;						//Data array
	short users;					//How many users accessing the drive
	struct request_queue *queue;	//Device request queue
	struct gendisk *gd;
}

static struct myblkdev *devices = null;

/*
 * Requests a major number for the block device and
 * tries to allocate memory to store the devices.
 */
static int __init start_module(void)
{
	int i;

	//Try to get a major number
	myblkdev_major = register_blkdev(myblkdev_major, DEV_NAME);
	if(myblkdev_major <= 0) {
		printk(KERN_WARNING "myblkdev: unable to get major number\n");
		return -EBUSY; //Why this error code?
	}

	//Allocate the devices array
	devices = kmalloc(num_devices * sizeof(struct myblkdev), GFP_KERNEL);
	if(devices == NULL) {
		unregister_blkdev(myblkdev_major, DEV_NAME);
		return -ENOMEM;
	}

	for(i = 0; i < num_devices; i++)
		setup_devices(devices + i, i);

	return 0;

}

/*
 * Called when the module is unloaded using the
 * rmmod command.
 */
static void __exit close_module(void)
{



}

module_init(start_module);
module_exit(close_module);