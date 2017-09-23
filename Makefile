obj-m := gpio-reflect.o
gpio-reflect-objs := driver.o signal.o


all:
	make -C /lib/modules/$$(uname -r)/build M=$$(pwd) modules
        
upload: all
	scp gpio-reflect.ko rpi3:/home/pi
        
clean:
	 make -C $(KERNEL_SRC) M=$(PWD) clean


