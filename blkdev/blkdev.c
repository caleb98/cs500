#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkdev.h>	// for block_device_operations struct
#include <linux/genhd.h>	// for gendisk struct
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/blk-mq.h>

/*
 * Provide module metadata
 */
MODULE_AUTHOR("Caleb Cassady <caleb.cassady@hotmail.com>");
MODULE_DESCRIPTION("A practice block device module");
MODULE_LICENSE("GPL");

#define DEVICE_NAME "bdev"
#define KERNEL_SECTOR_SIZE 512

static int bdev_major = 0;
static int dev_sector_size = 512;
static int num_sectors = 1024;
static int num_devices = 4;
static int bdev_minors = 16;

module_param(bdev_major, int, S_IRUGO);
module_param(dev_sector_size, int, S_IRUGO);
module_param(num_sectors, int, S_IRUGO);
module_param(num_devices, int, S_IRUGO);
module_param(bdev_minors, int, S_IRUGO);

struct bdev {
	int size;						// Device size (in sectors)
	u8 *data;						// Data array
	short users;					// Number of users
	short media_change;				// Flag for media changed
	spinlock_t lock;				// For mutual exclusion
	struct blk_mq_tag_set tag_set;	// Tag set for request queue
	struct request_queue *queue;	// Device request queue
	struct gendisk *gd;
	struct timer_list timer;		// For simulated media changes
};

static struct bdev *devices = NULL;

static void bdev_transfer(struct bdev *dev, unsigned long sector,
							unsigned long nsect, char *buffer, int write)
{
	unsigned long offset = sector * KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;

	printk(KERN_INFO "bdev: bdev_transfer()\n");

	// Make sure there's room to do the write
	if((offset + nbytes) > dev->size) {
		printk(KERN_NOTICE "bdev: beyond-end write (%ld %ld)\n", offset, nbytes);
		return;
	}

	if(write)
		memcpy(dev->data + offset, buffer, nbytes);
	else
		memcpy(buffer, dev->data + offset, nbytes);
}

/*
 * This code is heavily modified due to changes in the 
 * request_queue and request structures in the linux
 * kernel since ldd3 was published. 
 */
static blk_status_t bdev_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
	struct request *req = bd->rq;
	struct bdev *dev = req->rq_disk->private_data;
	struct bio_vec bvec;
	struct req_iterator iter;
	sector_t pos_sector = blk_rq_pos(req);
	void *buffer;

	printk(KERN_INFO "bdev: bdev_request()\n");

	// Start processing the request queue
	blk_mq_start_request(req);

	/*
	 * blk_rq_is_passthrough(req) is the new version
	 * blk_fs_request(req).
	 */
	if(blk_rq_is_passthrough(req)) {
		printk(KERN_NOTICE "bdev: Skip non-fs request\n");
		blk_mq_end_request(req, BLK_STS_IOERR);
		return BLK_STS_IOERR;
	}

	/*
	 * This macro replaces the while loop that was needed previously:
	 * while((req = elv_next_request(q)) != NULL) { 
	 *    ...
	 * }
	 */
	rq_for_each_segment(bvec, req, iter) {

		// req->current_nr_sectors
		size_t num_sector = blk_rq_cur_sectors(req);

		printk(KERN_NOTICE "bdev: Req device=%u\tdir=%d\tpos_sector=%lld\tnum_sector=%ld\n",
			(unsigned)(dev - devices), rq_data_dir(req),
			pos_sector, num_sector);

		buffer = page_address(bvec.bv_page) + bvec.bv_offset;
		bdev_transfer(dev, pos_sector, num_sector, buffer, rq_data_dir(req) == WRITE);
		pos_sector += num_sector;
	}

	// Finish processing the request queue
	blk_mq_end_request(req, BLK_STS_OK);
	return BLK_STS_OK;
}

static int bdev_open(struct block_device *bdev, fmode_t mode)
{
	struct bdev *dev = bdev->bd_disk->private_data;

	printk(KERN_INFO "bdev: bdev_open()\n");

	spin_lock(&dev->lock);

	if(!dev->users)
		check_disk_change(bdev);
	dev->users++;

	spin_unlock(&dev->lock);
	return 0;
}

static void bdev_release(struct gendisk *gd, fmode_t mode)
{
	struct bdev *dev = gd->private_data;

	printk(KERN_INFO "bdev: bdev_release()\n");

	spin_lock(&dev->lock);

	dev->users--;

	spin_unlock(&dev->lock);
}

static struct block_device_operations bdev_ops = {
	.owner			= THIS_MODULE,
	.open 			= bdev_open,
	.release 		= bdev_release,
};

static struct blk_mq_ops mq_ops = {
	.queue_rq = bdev_request,
};

static void setup_device(struct bdev *dev, int num)
{

	printk(KERN_INFO "bdev: creating device %d: ", num);

	/*
     * Get memory for the device's data field.
     * We use vmalloc since we'll likely want
     * an amount of data that is larger than
     * the size of a block.
     */
	memset(dev, 0, sizeof(struct bdev));
	dev->size = num_sectors * dev_sector_size;
	dev->data = vmalloc(dev->size);
	if(dev->data == NULL) {
		printk(KERN_NOTICE "bdev: vmalloc failure.\n");
		return;
	}

	// Initialize the spin lock used for mutual exclusion
	spin_lock_init(&dev->lock);

	// TODO: timer that invalidates device

	// Allocate request queue
	dev->queue = blk_mq_init_sq_queue(&dev->tag_set, &mq_ops, 128, BLK_MQ_F_SHOULD_MERGE);
	if(dev->queue == NULL) {
		if(dev->data) 
			vfree(dev->data);
		return;
	}
	blk_queue_max_segment_size(dev->queue, dev_sector_size);

	// Allocate and initialize gendisk struct
	dev->gd = alloc_disk(bdev_minors);
	if(dev->gd == NULL) {
		printk(KERN_NOTICE "bdev: alloc_disk failure.\n");
		if(dev->data)
			vfree(dev->data);
		return;
	}

	// Set up gendisk
	dev->gd->major = bdev_major;
	dev->gd->first_minor = num * bdev_minors;
	dev->gd->fops = &bdev_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf(dev->gd->disk_name, 32, "bdev%c", num + 'a');
	set_capacity(dev->gd, num_sectors * (dev_sector_size / KERNEL_SECTOR_SIZE));
	add_disk(dev->gd);

	printk(KERN_INFO "%s\n", dev->gd->disk_name);

}

/*
 * Requests a major number for the block device and
 * tries to allocate memory to store the devices.
 */
static int __init start_module(void)
{
	int i;

	printk(KERN_INFO "bdev: initializing\n");

	// Try to get a major number
	bdev_major = register_blkdev(bdev_major, DEVICE_NAME);
	if(bdev_major <= 0) {
		printk(KERN_WARNING "bdev: unable to get major number\n");
		return -EBUSY;
	}

	printk(KERN_INFO "bdev: got major number %d\n", bdev_major);

	// Allocate the devices array
	devices = kmalloc(num_devices * sizeof(struct bdev), GFP_KERNEL);
	if(devices == NULL) {
		unregister_blkdev(bdev_major, DEVICE_NAME);
		return -ENOMEM;
	}

	printk(KERN_INFO "bdev: allocated device memory\n");
	printk(KERN_INFO "bdev: %d devices requested\n", num_devices);

	// Set up each individual device
	for(i = 0; i < num_devices; i++)
		setup_device(devices + i, i);

	return 0;

}

/*
 * Called when the module is unloaded using the
 * rmmod command.
 */
static void __exit close_module(void)
{

	int i;

	printk(KERN_INFO "bdev: exiting module\n");

	for(i = 0; i < num_devices; i++) {
		struct bdev *dev = devices + i;

		if(dev->gd) 
			del_gendisk(dev->gd);

		if(dev->queue)
			blk_cleanup_queue(dev->queue);

		if(dev->data)
			vfree(dev->data);
	}

	unregister_blkdev(bdev_major, DEVICE_NAME);
	kfree(devices);

}

module_init(start_module);
module_exit(close_module);