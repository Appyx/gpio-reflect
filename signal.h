/*
 * signal.h
 *
 *  Created on: Jun 1, 2017
 *      Author: robert
 */

#ifndef SIGNAL_H_
#define SIGNAL_H_

#define SUCCESS 0
#define FAIL -1

#include <linux/list.h> //linked list
#include <linux/completion.h>

/*
 * Represents either a sent or a received signal.
 * The most fields can be set through the modules params
 */
struct signal {
	//general fields
	struct list_head list;
	char* name;
	int pin;

	//fields for receiving
	int timeout;
	unsigned int str_size;
	unsigned int size;
	int min_size;
	int max_size;

	//fields for sending
	unsigned int freq;
	int freq_div;

};

/**
 * Represents a single node of a signal
 */
struct sig_node {
	struct list_head node;
	unsigned long udelta;
	unsigned long mdelta;
	char* str;
	unsigned long str_size;
};

/**
 * Represents a receiver which is only active while receiving
 */
struct receiver {
	struct completion comp;
	struct timespec ts0;
	struct timer_list timer;
	int empty;
};

/**
 * Represents a sender which is only active while sending
 */
struct sender {
	struct list_head* ptr;
	struct list_head* last_ptr;
	struct hrtimer hr_timer;
	int lvl;
	int pin;
	unsigned long pwm_on;
	unsigned long pwm_off;
	int nopwm;
	ktime_t pwm_end;
	int pwm_flag;
	struct completion comp;
};

/**
 * This function clears all signal data, in a way that it can be reused later.
 */
void sig_clr(struct signal* sig);

/**
 * This function adds a node to the signal.
 * The added time is interpreted in microseconds.
 */
struct sig_node* sig_add(struct signal* sig, unsigned long val);

/**
 * This function allocates and initializes a signal which must be destroyed with sig_destroy()
 */
struct signal* sig_create(char* name, int pin, int delay, int min, int max,
		int freq, int freq_div);

/**
 *  THis function fills a signal with data in the format MILLI.MICRO|MILLI.MICRO|....
 */
int sig_fill(struct signal* sig, char*data);

/**
 * This function is the counterpart to sig_create().
 */
void sig_destroy(struct signal* sig);

/**
 * This function sends the given signal.
 * This call blocks until the signal is sent, or interrupted by the user.
 */
int sig_send(struct signal* sig);

/**
 * This function is only for debugging purposes.
 */
void sig_print(struct signal* sig);

/**
 * This function is usually called from interrupt context.
 * It adds a single node to the signal.
 */
void sig_receive_chunk(struct signal* sig, struct timespec* ts);

/**
 * This function allocates a char* which points to the string representation of the signal.
 * The returned char* must be freed after usage.
 */
char* sig_tostr(struct signal* sig);

/**
 * This function is called to start receiving a signal.
 * This call blocks until the signal is sent, or interrupted by the user.
 */
int sig_receive(struct signal* sig);

#endif /* SIGNAL_H_ */
