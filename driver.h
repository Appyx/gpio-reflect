/**
 * defines and variables
 */

#ifndef MY_HEADER_H
# define MY_HEADER_H

#define DEVICE_NAME "gpio-reflect"

/**
 * function prototypes
 */
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_close(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static irq_handler_t irq_handler(unsigned int irq, void *dev_id,
		struct pt_regs *regs);

#endif
