#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "pmod.h"

MODULE_AUTHOR("Caleb Cassady <caleb.cassady@hotmail.com>");
MODULE_DESCRIPTION("A practice driver module.");
MODULE_LICENSE("GPL");

static int module_major = MODULE_MAJOR;
static int module_minor = MODULE_MINOR;
static int num_devices = NUM_DEVICES;
static int data_block_size = DATA_BLOCK_SIZE;

module_param(module_major, int, S_IRUGO);
module_param(module_minor, int, S_IRUGO);
module_param(num_devices, int, S_IRUGO);
module_param(data_block_size, int, S_IRUGO);

static dev_t dev_number;
static struct pmod_dev *devices;
static int device_open = 0;

static void print_pmod_dev_info(struct pmod_dev *dev) 
{
	printk(KERN_INFO "pmod:\tdev->num_blocks = %d\n", dev->num_blocks);
}

/*
 * Creates the required pmod_block structs in the given device.
 * This method does not allocate space for the block_data field
 * in the pmod_data struct.
 */
static struct pmod_block *pmod_get_block(struct pmod_dev *dev, int block_num) 
{
	struct pmod_block *block;

	printk(KERN_INFO "pmod:\tpmod_get_block() called (getting block %d)\n", block_num);

	// If the first block of data isn't allocated, go ahead and get the memory
	if(!dev->data) {
		printk(KERN_INFO "pmod:\t\tpmod_dev didn't have any data, so attempting first block allocation...\n");
		dev->data = kmalloc(sizeof(struct pmod_block), GFP_KERNEL);
		if(dev->data == NULL)
			return NULL; // No memeory
		memset(dev->data, 0, sizeof(struct pmod_block));

		printk(KERN_INFO "pmod:\t\tallocation successful\n");

		// We added a block, so increase num of blocks
		dev->num_blocks++;	
	}

	block = dev->data;

	// Loop to create the rest of the required blocks
	while(0 < block_num--) {
		if(!block->next) {
			printk(KERN_INFO "pmod:\t\trequired block doesn't exist, attempting allocation...\n");
			block->next = kmalloc(sizeof(struct pmod_block), GFP_KERNEL);
			if(block->next == NULL)
				return NULL;
			memset(block->next, 0, sizeof(struct pmod_block));

			printk(KERN_INFO "pmod:\t\tallocation successful\n");

			// We added a block, so increase num of blocks
			dev->num_blocks++;
		}
		block = block->next;
	}

	return block;
}

/*
 * Clears out a device's blocks.
 */
static void pmod_trim(struct pmod_dev *dev)
{
	struct pmod_block *block, *next;

	// Free each pmod_block, and it's block_data if it has any.
	for(block = dev->data; block; block = next) {
		if(block->block_data) {
			kfree(block->block_data);
			block->block_data = NULL;
		}
		next = block->next;
		kfree(block);
	}

	// Set device blocks to 0
	dev->num_blocks = 0;
}

static int pmod_open(struct inode *inode, struct file *filp) 
{
	struct pmod_dev *device;

	printk(KERN_INFO "pmod: pmod_open() called.");

	// If the device is already open, we cannot open it again
	if(device_open) {
		printk(KERN_INFO "pmod:\t\tDevice in use. Access denied.\n");
		return -EBUSY;
	}

	/* 
	 * Move the pmod_dev struct for this device file into the
	 * filp private data field. This allows us to access the
	 * pmod_dev struct in relevant future calls.
	 */
	device = container_of(inode->i_cdev, struct pmod_dev, cdev);
	filp->private_data = device;

	// Increase internal mod use count
	device_open++;

	return 0;
}

static int pmod_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "pmod: pmod_release() called.\n");

	// Decrease internal mod use count
	device_open--;

	return 0;	
}

static ssize_t pmod_read(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	struct pmod_dev *dev = filp->private_data;
	struct pmod_block *block;
	int block_num, block_pos;

	printk(KERN_INFO "pmod: pmod_read() called (count: %ld, pos: %lld)\n", count, *pos);
	print_pmod_dev_info(dev);

	// Get block number and pos
	block_num = (long) *pos / data_block_size;
	block_pos = (long) *pos % data_block_size;

	printk(KERN_INFO "pmod:\tblock_num=%d, block_pos=%d\n", block_num, block_pos);

	// Make sure our device has the required number of blocks
	if(dev->num_blocks <= block_num) 
		return 0;

	// Get the block
	block = pmod_get_block(dev, block_num);

	// Make sure the block has data
	if(!block || !block->block_data)
		return 0;

	printk(KERN_INFO "pmod:\tread found block %p\n", block);

	// Only read to the end of this block
	if(count > data_block_size - block_pos) {
		count = data_block_size - block_pos;
		printk(KERN_INFO "pmod:\ttrimming count to %ld\n", count);
	}

	printk(KERN_INFO "pmod:\tcopying data from kernel buffer to user buffer\n");

	// Try to copy the data
	if(copy_to_user(buff, block->block_data + block_pos, count)) {
		return -EFAULT;
	}

	// Increase file position pointer
	*pos += count;

	print_pmod_dev_info(dev);

	// Return num of bytes read
	return count;
}

static ssize_t pmod_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
	struct pmod_dev *dev = filp->private_data;
	struct pmod_block *block;
	int block_num, block_pos;

	printk(KERN_INFO "pmod: pmod_write() called (count: %ld, pos: %lld)\n", count, *pos);
	print_pmod_dev_info(dev);

	// If the write pos is 0, empty the device data 
	if(*pos == 0) {
		printk(KERN_INFO "pmod:\twriting at position 0, so we're emptying the device first");
		pmod_trim(dev);
	}

	// Get block number and pos
	block_num = (long) *pos / data_block_size;
	block_pos = (long) *pos % data_block_size;

	printk(KERN_INFO "pmod:\tblock_num=%d, block_pos=%d\n", block_num, block_pos);

	// Grab pointer to block based on block number
	block = pmod_get_block(dev, block_num);
	if(!block) 
		return -ENOMEM;

	printk(KERN_INFO "pmod:\twrite found block %p\n", block);

	// Make sure block data memory is allocated
	if(!block->block_data) {
		printk(KERN_INFO "pmod:\tblock data was not allocated. attempting allocation...\n");
		block->block_data = (char *) kmalloc(data_block_size * sizeof(char), GFP_KERNEL);
		if(!block->block_data)
			return -ENOMEM;
		memset(block->block_data, 0, data_block_size * sizeof(char));
		printk(KERN_INFO "pmod:\tblock data allocated (%ld)\n", data_block_size * sizeof(char));
	}

	// Only write to the end of the block
	if(count > data_block_size - block_pos) {
		count = data_block_size - block_pos;
		printk(KERN_INFO "pmod:\ttrimming count to %ld\n", count);
	}

	printk(KERN_INFO "pmod:\tcopying data from user buffer to kernel buffer\n");

	// Try to copy data
	if(copy_from_user(block->block_data + block_pos, buf, count)) {
		return -EFAULT;
	}

	// Increase file position pointer
	*pos += count;

	print_pmod_dev_info(dev);

	// Return num of bytes read
	return count;
}

static struct file_operations pmod_fops = {
	.owner =		THIS_MODULE,
	.open =			pmod_open,
	.release = 		pmod_release,
	.read =			pmod_read,
	.write =		pmod_write,
};

static void init_pmod_dev(struct pmod_dev *dev, int devnum) 
{
	int error;
	dev_t this_dev = MKDEV(module_major, module_minor + devnum);

	// Device is closed
	dev->device_open = 0;

	// Device starts with 0 blocks
	dev->num_blocks = 0;

	// Initialize and add the character device
	cdev_init(&dev->cdev, &pmod_fops);
	dev->cdev.owner = THIS_MODULE;
	error = cdev_add(&dev->cdev, this_dev, 1);

	// Check for errors
	if(error) {
		printk(KERN_WARNING "pmod: Error %d adding pmod device %d\n", error, devnum);
	}
	else {
		printk(KERN_INFO "pmod: Added pmod device %d:%d\n", module_major, MINOR(this_dev));
	}
}

/*
 * Module init and cleanup.
 */
int init_module(void)
{
	int result;
	int i;
	printk(KERN_INFO "pmod: Starting module.\n");

	/*
	 * Get the minor numbers for our device. If module_major
	 * has been specified, ask for that major number. Otherwise
	 * just get a dynamic major number.
	 */
	if(module_major == 0) {
		result = alloc_chrdev_region(&dev_number, module_minor, num_devices, DEVICE_NAME);
		module_major = MAJOR(dev_number);
	}
	else {
		dev_number = MKDEV(module_major, module_minor);
		result = register_chrdev_region(dev_number, num_devices, DEVICE_NAME);
	}

	// Make sure we got device number successfully.
	if(result < 0) {
		printk(KERN_WARNING "pmod: can't get major %d\n", module_major);
		return result;
	}

	printk(KERN_INFO "pmod: Registered device region %d:%d (len: %d)\n", 
		module_major, module_minor, module_minor + num_devices);

	// Allocate space for the devices
	devices = (struct pmod_dev *) kmalloc(num_devices * sizeof(struct pmod_dev), GFP_KERNEL);
	if(!devices) {
		printk(KERN_WARNING "pmod: unable to allocate memory for devices\n");
		cleanup_module();
		return -ENOMEM;
	}
	memset(devices, 0, num_devices * sizeof(struct pmod_dev));

	// Init each device
	for(i = 0; i < num_devices; i++) {
		init_pmod_dev(&devices[i], i);
	}

	return 0;
}

void cleanup_module(void)
{
	int i;

	// Cleanup individual pdev devices
	if(devices) {
		for(i = 0; i < num_devices; i++) {
			pmod_trim(devices + i);
			cdev_del(&devices[i].cdev);
		}
		kfree(devices);
	}

	//Unregister character device number region
	unregister_chrdev_region(dev_number, num_devices);
	printk(KERN_INFO "pmod: Exiting module.\n");
}