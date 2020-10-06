#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h> // for put_user

MODULE_AUTHOR("Caleb Cassady <caleb.cassady17@gmail.com>");
MODULE_DESCRIPTION("A practice driver module.");
MODULE_LICENSE("GPL");

/*
 * Prototypes for file_operations struct
 */
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "chardev"
#define BUF_LEN 80

/*
 * Global variables are declared static so as not to
 * interfere with other values in the kernel space.
 */
static int Major;
static int Device_Open = 0;

static char msg[BUF_LEN];
static char* msg_Ptr;

static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

/*
 *
 */
int init_module(void)
{
	printk(KERN_INFO "MyDriver: Setting up chardev driver.\n");

	Major = register_chrdev(0, DEVICE_NAME, &fops);

	if(Major < 0) {
		printk(KERN_ALERT "Registering char device failed with %d\n", Major);
		return Major;
	}

	printk(KERN_INFO "MyDriver: I was assigned major number %d. To talk to\n", Major);
	printk(KERN_INFO "the driver, create a dev file with \n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
	printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	return SUCCESS;
}

void cleanup_module(void)
{
	//Unregister the device
	unregister_chrdev(Major, DEVICE_NAME);
}

// module_init(start_module);
// module_exit(exit_module);

/*
 * Device method implementations
 */

//Called when a process tries to open the device file
//for example, "cat /dev/mycharfile"
static int device_open(struct inode* inode, struct file* file) 
{
	static int counter = 0;

	if(Device_Open)
		return -EBUSY;

	Device_Open++;
	sprintf(msg, "I already told you %d times Hello World!\n", counter++);
	msg_Ptr = msg;

	//Increase the module count to signal that
	//our driver is in use.
	try_module_get(THIS_MODULE);

	return SUCCESS;
}

//Called when a process closes the device file
static int device_release(struct inode* inode, struct file* file) 
{
	Device_Open--;

	//Decrement the module count so we can remove
	//the module without complications.
	module_put(THIS_MODULE);

	return 0;
}

//Called when a process, which already opened the dev file,
//attempts to read from it.
static ssize_t device_read(struct file* filp,	//see include/linux/fs.h
						   char* buffer,		//buffer to fill with data
						   size_t length,		//length of buffer
						   loff_t* offset)
{
	//Number of bytes actually written to the buffer
	int bytes_read = 0;

	//If we're at the end of the message,
	//return 0 signifying the EOF
	if(*msg_Ptr == 0) {
		return 0;
	}

	while(length && *msg_Ptr) {

		/*
		 * The buffer is in the user data segment, not the kernel
		 * segment. So "*" assignment wont work. We have to use
		 * put_user which copies data from the kernel data segment
		 * into the user data segment.
		 */
		put_user(*(msg_Ptr++), buffer++);

		length--;
		bytes_read++;
	}

	//Most read functions return the number of bytes put into the buffer
	return bytes_read;
}

static ssize_t device_write(struct file* filp, const char* buff, size_t len, loff_t* off) 
{
	printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}