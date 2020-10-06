/*
 * Include necessary files for kernel module.
 * The main files here are kernel.h and module.h.
 *
 * The moduleparam.h file allows us to specify
 * parameters that may be set by the user for use
 * in our module.
 * 
 * The init.h module provides macros for defining
 * module initialization and cleanup methods. These
 * macros allow the kernel to discard the init
 * function and free its memory after the driver
 * has been loaded (for any built-in drivers.)
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

/*
 * Provide module metadata
 */
MODULE_AUTHOR("Caleb Cassady <caleb.cassady17@gmail.com>");
MODULE_DESCRIPTION("A practice driver module.");
MODULE_LICENSE("GPL");

/*
 * List module parameters.
 * Here, we demonstrate how to create standard data 
 * types and arrays as module parameters.
 *
 * Info about these parameters can be obtained by
 * running modinfo on the kernel object file.
 */
static int sum = 10;
module_param(sum, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(sum, "Value to sum to");

static int pow[2] = {-1, -1};
static int pow_argc = 0;
module_param_array(pow, int, &pow_argc, 0000);
MODULE_PARM_DESC(pow, "Values to pow");

/*
 * Called when the module is loaded using the insmod
 * command.
 */
static int __init start_module(void)
{

	printk(KERN_INFO "ExampleModule: Starting example module.\n");

	int total = 0;
	int i;
	for(i = 1; i <= sum; i++) {
		total += i;
	}
	printk(KERN_INFO "ExampleModule: Sum from 1 to %d is %d\n", sum, total);

	if(pow_argc < 2) {
		printk(KERN_INFO "ExampleModule: Pow will not be calculated "
			"- requires two args. (%d provided)\n", pow_argc);
	}
	else {
		total = pow[0];
		for(i = 1; i < pow[1]; i++) {
			total *= pow[0];
		}
		printk(KERN_INFO "ExampleModule: %d^%d = %d\n", pow[0], pow[1], total);
	}

	return 0;

}

/*
 * Called when the module is unloaded using the
 * rmmod command.
 */
static void __exit close_module(void)
{

	printk(KERN_INFO "ExampleModule: Exiting example module.\n");

}

module_init(start_module);
module_exit(close_module);