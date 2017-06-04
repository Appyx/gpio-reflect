#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h> //major minor number
#include <linux/fs.h>
#include <linux/device.h> //device class
#include <linux/cdev.h> //for device file
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/slab.h> //kmalloc and free
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include "driver.h"
#include "signal.h"

/**
 * module params
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_DESCRIPTION("Utility to record and send GPIO-signals");

static short int in = 15; //input pin
static short int out = 18; //output pin
static int timeout = 1000; //delay for detecting signals
static int max_size = 200; //maximum size of signal
static int min_size = 50; //minimum size of signal
static int freq = 36; //frequency of pwm in kHz
static int pwm = 2; //frequency divider for pwm negative=bottom halve of pulse, positive=top halve of pulse, 0=no pwm

module_param(in, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(out, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(timeout, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(max_size, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(min_size, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(freq, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(pwm, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

/**
 * driver specific fields
 */
static struct signal* send; //instance of the sending signal
static struct signal* recv; //instance of the receiving signal

static unsigned int irq; //interrupt number
static dev_t dev; //major minor number
static struct cdev c_dev; //device file
static struct class *cl; //device class
static struct device *dv;
struct mutex lock; //lock for single user access
static char* read_buf = NULL; //buffer for reading the signal back to the user
static int read_pos = 0; //the  current position of the buffer (used if small buffers are used)



/**
 * File Operations
 *
 */
static struct file_operations fops = { .owner = THIS_MODULE,
		.read = device_read, .write = device_write, .open = device_open,
		.release = device_close };

static int device_open(struct inode *i, struct file *f) {
	if (mutex_lock_interruptible(&lock)) {
		return -ERESTARTSYS;
	}
	return SUCCESS;
}
static int device_close(struct inode *i, struct file *f) {
	read_pos = 0; //reset position
	read_buf = NULL; //reset buffer
	mutex_unlock(&lock);
	return SUCCESS;
}
static ssize_t device_read(struct file *f, char __user *buf, size_t len,
		loff_t *off) {
	int res;
	int bytes = 0;
	char* str;
	if (read_buf == NULL) { //if first call
		if (request_irq(irq, (irq_handler_t) irq_handler,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "gpioref_handler", NULL)) {
			return -EFAULT;
		}
		res = sig_receive(recv);
		free_irq(irq, NULL);
		if (res > 0) {
			str = sig_tostr(recv);
			read_buf = kzalloc(recv->str_size, GFP_KERNEL);
			memcpy(read_buf, str, recv->str_size);
			sig_clr(recv);
			kfree(str); //always free the big string
		} else {
			return res;
		}
	}
	while (read_buf[read_pos] && len) {
		put_user(read_buf[read_pos], buf++);
		len--;
		read_pos++;
		bytes++;
	}
	return bytes;
}
static ssize_t device_write(struct file *f, const char __user *buf, size_t len,
		loff_t *off) {
	char* data;

	data = kzalloc(len, GFP_KERNEL);
	if (copy_from_user(data, buf, len)) {
		kfree(data);
		return -EFAULT;
	}
	if (sig_fill(send, data)) {
		sig_clr(send);
		kfree(data);
		return -EINVAL;
	}
	if (sig_send(send)) { //blocking call
		sig_clr(send);
		kfree(data);
		return -ERESTARTSYS;
	}
	//sig_print(send);
	sig_clr(send);
	return len;
}

/**
 * interrupt routine
 */
static irq_handler_t irq_handler(unsigned int irq, void *dev_id,
		struct pt_regs *regs) {
	struct timespec ts;
	getnstimeofday(&ts);
	sig_receive_chunk(recv, &ts);
	return (irq_handler_t) IRQ_HANDLED;
}

/**
 * init and exit functions
 */

static int __init gpio_reflect_init(void) {
	if (alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME)) {
		printk(KERN_INFO "alloc_chrdev_region failed!\n");
		return FAIL;
	}
	cl = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(cl)) {
		printk(KERN_INFO "class_create failed!\n");
		unregister_chrdev_region(dev, 1);
		return PTR_ERR(cl);
	}
	dv= device_create(cl, NULL, dev, NULL, DEVICE_NAME);
	if (IS_ERR(dv)) {
		printk(KERN_INFO "device_create failed!\n");
		class_destroy(cl);
		unregister_chrdev_region(dev, 1);
		return PTR_ERR(dv);
	}
	cdev_init(&c_dev, &fops);
	if (cdev_add(&c_dev, dev, 1)) {
		device_destroy(cl, dev);
		class_destroy(cl);
		unregister_chrdev_region(dev, 1);
		return -FAIL;
	}
	if (!gpio_is_valid(out) || !gpio_is_valid(in)) {
		printk(KERN_INFO "Invalid gpio!\n");
		device_destroy(cl, dev);
		class_destroy(cl);
		unregister_chrdev_region(dev, 1);
		return FAIL;
	}
	gpio_request(out, DEVICE_NAME);
	gpio_direction_output(out, false);
	gpio_request(in, DEVICE_NAME);
	gpio_direction_input(in);

	irq = gpio_to_irq(in);
	mutex_init(&lock);

	send = sig_create("send", out, timeout, min_size, max_size, freq, pwm);
	recv = sig_create("receive", in, timeout, min_size, max_size, freq, pwm);

	printk(KERN_INFO "%s loaded\n", DEVICE_NAME);
	printk(KERN_INFO "in-pin: %i\n", in);
	printk(KERN_INFO "out-pin: %i\n", out);
	printk(KERN_INFO "timeout: %i\n", timeout);
	printk(KERN_INFO "min-size: %i\n", min_size);
	printk(KERN_INFO "max-size: %i\n", max_size);
	printk(KERN_INFO "frequency: %i\n", freq * 1000);
	printk(KERN_INFO "frequency-divider: %i\n", pwm);
	return SUCCESS;
}

static void __exit gpio_reflect_cleanup(void) {
	gpio_free(in);
	gpio_set_value(out, 0);
	gpio_free(out);

	cdev_del(&c_dev);
	device_destroy(cl, dev);
	class_destroy(cl);
	unregister_chrdev_region(dev, 1);

	sig_destroy(recv);
	sig_destroy(send);
	printk(KERN_INFO "%s unloaded\n", DEVICE_NAME);
}

module_init(gpio_reflect_init);
module_exit(gpio_reflect_cleanup);

