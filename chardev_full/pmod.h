#ifndef _PMOD_H_
#define _PMOD_H_

#include <linux/cdev.h>

#define DEVICE_NAME "pmod"
#define MODULE_MAJOR 0
#define MODULE_MINOR 0
#define NUM_DEVICES 1
#define DEVICE_BUFFER_SIZE 32

struct pmod_dev {
	char *data;
	int device_open;
	struct cdev cdev;
};

#endif