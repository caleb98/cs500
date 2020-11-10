#ifndef _PMOD_H_
#define _PMOD_H_

#include <linux/cdev.h>
#include <linux/mutex.h>

#define DEVICE_NAME "pmod"
#define MODULE_MAJOR 0
#define MODULE_MINOR 0
#define NUM_DEVICES 1
#define DATA_BLOCK_SIZE 32

struct pmod_block {
	char *block_data;
	struct pmod_block *next;
};

struct pmod_dev {
	struct pmod_block *data;
	int num_blocks;
	int device_open;
	struct mutex mut;
	struct cdev cdev;
};

#endif