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
static int device_buffer_size = DEVICE_BUFFER_SIZE;

module_param(module_major, int, S_IRUGO);
module_param(module_minor, int, S_IRUGO);
module_param(num_devices, int, S_IRUGO);
module_param(device_buffer_size, int, S_IRUGO);

static dev_t dev_number;
static struct pmod_dev *devices;
static int device_open = 0;

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

	printk(KERN_INFO "pmod: pmod_read() called (count: %ld, pos: %lld)\n", count, *pos);

	// Make sure reading pos is within buffer
	if(*pos >= device_buffer_size) {
		printk(KERN_INFO "pmod:\t\tRead position larger than buffer. No bytes read.\n");
		return 0;
	}

	// Make sure reading bound is within buffer
	if(*pos + count > device_buffer_size) {
		count = device_buffer_size - *pos;
		printk(KERN_INFO "pmod:\t\tRead bounds outside buffer. Trimming read size: %ld\n", count);
	}

	// Try to copy the data
	if(copy_to_user(buff, dev->data + *pos, count)) {
		return -EFAULT;
	}

	// Increase file position pointer
	*pos += count;

	// Return num of bytes read
	return count;
}

static ssize_t pmod_write(struct file *filp, const char __user *buff, size_t count, loff_t *pos)
{
	struct pmod_dev *dev = filp->private_data;

	printk(KERN_INFO "pmod: pmod_write() called (count: %ld, pos: %lld)\n", count, *pos);

	// Clear current device buffer if we're writing to the start of the file
	if(*pos == 0)
		memset(dev->data, 0, device_buffer_size);

	// Make sure writing pos is within the buffer
	if(*pos >= device_buffer_size) {
		printk(KERN_INFO "pmod:\t\tWrite position larger than buffer. Returning error.\n");
		return -ENOMEM; // No more memory to write in the buffer
	}

	// Make sure writing bound is within the buffer
	if(*pos + count > device_buffer_size) {
		count = device_buffer_size - *pos;
		printk(KERN_INFO "pmod:\t\tWrite bounds outside buffer. Trimming write size: %ld\n", count);
	}

	// Try to copy data
	if(copy_from_user(dev->data + *pos, buff, count)) {
		return -EFAULT;
	}

	// Increase file position pointer
	*pos += count;

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

	// Allocate memory for device data
	dev->data = kmalloc(device_buffer_size, GFP_KERNEL);
	if(!dev->data) {
		printk(KERN_WARNING "pmod: Unable to allocate memory for device %d data\n", devnum);
		return;
	}
	memset(dev->data, 0, device_buffer_size);

	// Initialize and add the character device
	cdev_init(&dev->cdev, &pmod_fops);
	dev->cdev.owner = THIS_MODULE;
	error = cdev_add(&dev->cdev, this_dev, 1);

	// Check for errors
	if(error) {
		printk(KERN_WARNING "pmod: Error %d adding pmod device %d\n", error, devnum);
		kfree(dev->data);
	}
	else {
		printk(KERN_INFO "pmod: Added pmod device %d:%d\n", module_major, MINOR(this_dev));
	}
}

static void cleanup_pmod_dev(struct pmod_dev *dev) 
{
	// Cleanup device data memory
	if(dev->data) 
		kfree(dev->data);
	// Free cdev registration
	cdev_del(&dev->cdev);
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
			cleanup_pmod_dev(&devices[i]);
		}
		kfree(devices);
	}

	//Unregister character device number region
	unregister_chrdev_region(dev_number, num_devices);
	printk(KERN_INFO "pmod: Exiting module.\n");
}