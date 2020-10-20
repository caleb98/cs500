/*
 * Full example of proc file creation using 
 * individual functions for each of the 
 * seq_operations.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h> 	//for kmalloc

#define procfs_name "myprocfs"

static void print_inf(char* data, loff_t* iter, loff_t* pos)
{
	printk(KERN_INFO "MyProcfs: %s\t(Iter:\t%p->%lld,\tPos:%p->%lld)\n", 
		data, iter, *iter, pos, *pos);

}

/*
 * Takes a file and position as an argument and returns
 * an iterator used to traverse the proc file.
 * In this case, our data structure for the iterator is
 * just an loff_t value holding the current reading pos.
 * For more complicated implementations, we will usually
 * want to check for a "past end of file" condition and 
 * return NULL if necessary.
 */
static void* myproc_start(struct seq_file* s, loff_t* pos) 
{
	/*
	 * Try to allocate memory for our iterator over the 
	 * proc file.
	 */
	loff_t* spos = kmalloc(sizeof(loff_t), GFP_KERNEL);
	if(!spos) 
		return NULL;

	print_inf("STARTING", spos, pos);

	//Set our iterator to point to the position requested
	*spos = *pos;
	return spos;
}

/*
 * This function is used to move the iterator forward to
 * the next position by one. More complicated modules will
 * generally step through a data structure. Returns a new
 * iterator, or NULL if the sequence is complete.
 */
static void* myproc_next(struct seq_file* s, void* v, loff_t* pos) 
{
	//Create our new iterator at the position of the old one
	loff_t* spos = (loff_t*) v;
	//Set the position to be 1 + old position
	*pos = ++(*spos);

	print_inf("    NEXT", spos, pos);

	//Return the new iterator
	return spos;
}

/*
 * Called once the iteration over the proc file is complete.
 * As expected, it should clean up any memory allocated by
 * the iterator (in this case, we can just free v since we
 * allocated it with kmalloc in the start function.)
 */
static void myproc_stop(struct seq_file* s, void* v)
{
	printk(KERN_INFO "MyProcfs: STOPPING\n\n");

	//v is the loff_t* we created in myproc_start
	kfree(v);
}

/*
 * Formats the object currently pointed to by the iterator
 * for output.
 */
static int myproc_show(struct seq_file* s, void* v) 
{
	loff_t* pos = v;
	seq_printf(s, "%lld\n", (long long) *pos);
	return 0;
}

/*
 * Structure that bundles all our relevant seq_ops
 * into one data structure that can be passed to the
 * kernel functions.
 */ 
static const struct seq_operations myseq_ops = {
	.start = myproc_start,
	.next  = myproc_next,
	.stop  = myproc_stop,
	.show  = myproc_show,
};

/*
 * Last function that we must implement for the
 * file_ops struct. This will just call seq_open, which
 * takes the seq_ops struct we just made and gets
 * ready to iterate through the virtual file.
 */
static int myproc_open(struct inode* inode, struct file* file)
{
	return seq_open(file, &myseq_ops);
}

/*
 * Bundles the relevant functions for our proc file.
 * Used when creating the proc file in myproc_init.
 */
static const struct file_operations myproc_fops = {
	.owner   = THIS_MODULE,
	.open    = myproc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
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