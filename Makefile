obj-m := gpio-reflect.o
gpio-reflect-objs := driver.o signal.o


all:
	make -C /lib/modules/$$(uname -r)/build M=$$(pwd) modules
        
install:all
	make -C /lib/modules/$$(uname -r)/build M=$$(pwd) modules_install
        
clean:
	 make -C $(KERNEL_SRC) M=$(PWD) clean


