/*
 * Simple example of how to create a "file" in /proc
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define procfs_name "myprocfs"

static int myproc_count = 0;

/*
 * The "meat" of this simple proc file reading. Just
 * prints out a message with our count value.
 */
static int myproc_show(struct seq_file* m, void* v) 
{
	seq_printf(m, "Hello #%d!\n", myproc_count++);
	return 0;
}

/*
 * The "open" function that our proc fs file uses. This simply
 * calls single_open (a function defined in linux/seq_file.h
 * made for reading virtual files) and passes our myproc_show
 * function as a parameter. The single_open function will then
 * use myproc_show to create the appropriate output for our 
 * virtual proc file.
 * See https://github.com/torvalds/linux/blob/v4.9/Documentation/filesystems/seq_file.txt
 * for documentation of seq_file.h and the single_open 
 * function.
 */
static int myproc_open(struct inode* inode, struct file* file) 
{
	return single_open(file, myproc_show, NULL);
}

/*
 * Bundles the relevant functions for our proc file.
 * Used when creating the proc file in myproc_init.
 */
static const struct file_operations myproc_fops = {
	.owner = THIS_MODULE,
	.open = myproc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * Init and cleanup methods (same as any other module.)
 */
static int __init myproc_init(void) 
{
	proc_create(procfs_name, 0, NULL, &myproc_fops);
	return 0;
}

static void __exit myproc_exit(void)
{
	remove_proc_entry(procfs_name, NULL);
}

MODULE_LICENSE("GPL");
module_init(myproc_init);
module_exit(myproc_exit);