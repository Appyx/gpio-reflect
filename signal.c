#include <linux/slab.h> //kmalloc and free
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/delay.h> //udelay and mdelay
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include "signal.h"

/**
 * Temporary objects during sending or receiving
 */
struct sender* sender;
struct receiver* receiver;

struct signal* sig_create(char* name, int pin, int delay, int min, int max,
		int freq, int freq_div) {
	struct signal* sig;
	sig = kzalloc(sizeof(*sig), GFP_KERNEL);
	sig->name = name;
	sig->pin = pin;
	sig->timeout = delay;
	sig->str_size = 0;
	sig->size = 0;
	sig->freq = freq;
	sig->freq_div = freq_div;
	sig->min_size = min;
	sig->max_size = max;
	INIT_LIST_HEAD(&sig->list);
	return sig;
}

void sig_destroy(struct signal* sig) {
	sig_clr(sig);
	list_del(&sig->list);
	kfree(sig);
}

void sig_receive_chunk(struct signal* sig, struct timespec* ts) {
	struct timespec dif;
	if (receiver == NULL) {
		return;
	}
	dif = timespec_sub(*ts, receiver->ts0);
	receiver->ts0 = *ts;

	if (receiver->empty) { //skip first
		receiver->empty = false;
		return;
	} else {
		sig_add(sig, dif.tv_nsec / 1000);
		mod_timer(&receiver->timer, jiffies + msecs_to_jiffies(sig->timeout));
	}
}

static void timer_callback(struct timer_list *t) {
	complete(&receiver->comp);
}

int sig_receive(struct signal* sig) {
	int ret;
	receiver = kzalloc(sizeof(*receiver), GFP_KERNEL);
	init_completion(&receiver->comp);
	getnstimeofday(&receiver->ts0);
	timer_setup(&receiver->timer, timer_callback, 0);
	receiver->empty = true;
	if (wait_for_completion_interruptible(&receiver->comp)) {
		del_timer(&receiver->timer);
		kfree(receiver);
		receiver = NULL;
		return -ERESTARTSYS;
	}

	ret = sig->str_size;
	printk(KERN_INFO "signal recorded\n");
	if (sig->size % 2 != 1 || sig->size > sig->max_size
			|| sig->size < sig->min_size) {
		printk(KERN_WARNING "Signal recording failed (size=%i)\n", sig->size);
		ret = -EINVAL;
		sig_clr(sig);
	}
	del_timer(&receiver->timer);
	kfree(receiver);
	receiver = NULL;
	return ret;
}

struct sig_node* sig_add(struct signal* sig, unsigned long val) {
	struct sig_node* t;
	int buf_size = 11; //the estimated buffer size for 99999.XXXX| so max. 99 seconds per entry
	t = kzalloc(sizeof(*t), GFP_KERNEL);

	t->udelta = 0;
	t->mdelta = 0;

	if (val < 1000) {
		t->udelta = val;
	} else {
		t->mdelta = val / 1000;
		t->udelta = val % 1000;
	}
	INIT_LIST_HEAD(&t->node);
	list_add_tail(&t->node, &sig->list);
	//pre-calculate the resulting string for each added node
	t->str = kzalloc(buf_size, GFP_KERNEL);
	t->str_size = scnprintf(t->str, buf_size, "%lu.%lu|", t->mdelta, t->udelta);
	sig->str_size += t->str_size;
	sig->size++;

	return t;
}

void sig_clr(struct signal* sig) {
	struct sig_node* entry = NULL;
	struct list_head* ptr = NULL;
	struct list_head* ptr_next = NULL;
	list_for_each_safe(ptr,ptr_next,&sig->list)
	{
		entry = list_entry(ptr, struct sig_node, node);
		list_del(&entry->node);
		kfree(entry->str);
		kfree(entry);
	}
	sig->str_size = 0;
	sig->size = 0;
}

void sig_print(struct signal* sig) {
	char* str;

	printk(KERN_INFO "signal: (name: %s, size: %i, pin: %i, timeout: %i)\n",
			sig->name, sig->size, sig->pin, sig->timeout);
	str = sig_tostr(sig);
	printk(KERN_INFO "%s\n", str); //printk may not print the whole string
	kfree(str);
}

/**
 * The returned char* MUST be freed!
 */
char* sig_tostr(struct signal* sig) {
	struct sig_node* entry = NULL;
	struct list_head* ptr = NULL;
	char* res = kzalloc(sig->str_size, GFP_KERNEL);

	list_for_each(ptr,&sig->list)
	{
		//use the pre-calculated strings and size
		entry = list_entry(ptr, struct sig_node, node);
		strcat(res, entry->str);
	}
	if (sig->str_size > 0) {
		*(res + sig->str_size - 1) = '\0'; //remove last |
	}
	return res;
}

int sig_fill(struct signal* sig, char*data) {
	unsigned long res, val;
	char* token;
	char* tmp_token;
	char* m;
	char* u;

	token = strsep(&data, "|");

	if (token == NULL) {
		return FAIL;
	}

	while (token != NULL) {
		tmp_token = kzalloc(strlen(token) + 1, GFP_KERNEL);
		strcpy(tmp_token, token);
		m = strsep(&tmp_token, ".");
		u = strsep(&tmp_token, ".");

		if (m != NULL && !kstrtoul(m, 10, &res)) {
			val = res * 1000;
			if (res > 99999)
				return FAIL;
		} else {
			return FAIL;
		}
		if (u != NULL && !kstrtoul(u, 10, &res)) {
			if (res >= 1000) {
				return -1;
			}
			val += res;
		} else {
			return FAIL;
		}
		kfree(tmp_token);
		sig_add(sig, val);
		token = strsep(&data, "|");
	}
	return 0;
}

static enum hrtimer_restart hrtimer_callback(struct hrtimer *timer) {
	struct sig_node* entry = NULL;
	ktime_t time;
	int gpio_val;

	if (sender->ptr == sender->last_ptr) { //stop everything if last
		gpio_set_value(sender->pin, 0);
		complete(&sender->comp);
		kfree(sender);
		return HRTIMER_NORESTART;
	}

	if (sender->lvl == 1) {
		if (sender->nopwm == 0) { //do pwm
			if (sender->pwm_flag == 1) { //pwm start
				entry = list_entry(sender->ptr, struct sig_node, node);
				sender->pwm_flag = 0;
				sender->pwm_end = ktime_get();
				sender->pwm_end = ktime_add_ms(sender->pwm_end, entry->mdelta);
				sender->pwm_end = ktime_add_us(sender->pwm_end, entry->udelta);
			}
			gpio_val = gpio_get_value(sender->pin);
			gpio_set_value(sender->pin, !gpio_val);

			if (ktime_after(ktime_get(), sender->pwm_end)) {
				sender->lvl = 0; //executes the next if statement immediately
				sender->pwm_flag = 1; //reset for the next time
				sender->ptr = sender->ptr->next;
			} else {
				hrtimer_start(&sender->hr_timer, ktime_set(0, sender->pwm_on),
						HRTIMER_MODE_REL);
			}
		} else { //do no pwm
			entry = list_entry(sender->ptr, struct sig_node, node);
			time = ktime_set(0, 0);
			time = ktime_add_ms(time, entry->mdelta);
			time = ktime_add_us(time, entry->udelta);
			sender->lvl = 0;
			gpio_set_value(sender->pin, 1);
			sender->ptr = sender->ptr->next;
			hrtimer_start(&sender->hr_timer, time, HRTIMER_MODE_REL);
			return HRTIMER_NORESTART;
		}
	}
	if (sender->ptr == sender->last_ptr) { //stop everything if last
		gpio_set_value(sender->pin, 0);
		complete(&sender->comp);
		kfree(sender);
		return HRTIMER_NORESTART;
	}
	if (sender->lvl == 0) { //do no pwm
		entry = list_entry(sender->ptr, struct sig_node, node);
		time = ktime_set(0, 0);
		time = ktime_add_ms(time, entry->mdelta);
		time = ktime_add_us(time, entry->udelta);
		sender->lvl = 1;
		gpio_set_value(sender->pin, 0);
		sender->ptr = sender->ptr->next;
		hrtimer_start(&sender->hr_timer, time, HRTIMER_MODE_REL);
	}

	return HRTIMER_NORESTART;
}

int sig_send(struct signal* sig) {

	unsigned long pulse = 1000000 / sig->freq; //time of one pwm pulse in nanosecs
	int tmp_div;

	sender = kzalloc(sizeof(*sender), GFP_KERNEL);
	init_completion(&sender->comp);
	sender->lvl = 1; //first pulse is always on
	sender->ptr = sig->list.next;
	sender->last_ptr = &sig->list;
	sender->pin = sig->pin;

	if (sig->freq_div > 0) { //diver divides lower half
		sender->nopwm = 0;
		sender->pwm_on = pulse / sig->freq_div; //divider calculates on period
		sender->pwm_off = pulse - sender->pwm_on; //rest is off period
	}
	if (sig->freq_div == 0) { //pwm off
		sender->nopwm = 1;
	}
	if (sig->freq_div < 0) { //diver divides upper half
		tmp_div = sig->freq_div * -1;
		sender->nopwm = 0;
		sender->pwm_off = pulse / sig->freq_div; //divider calculates off period
		sender->pwm_on = pulse - sender->pwm_on; //rest is on period
	}

	sender->pwm_flag = 1;

	hrtimer_init(&sender->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sender->hr_timer.function = &hrtimer_callback;

	hrtimer_start(&sender->hr_timer, ktime_set(0, 1), HRTIMER_MODE_REL);
	return wait_for_completion_interruptible(&sender->comp);
}

