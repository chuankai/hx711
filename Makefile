KERNELDIR := /home/chuankai/projects/rpi_kernel/linux
PWD := $(shell pwd)
CROSS_COMPILE := /home/chuankai/tools/x-tools6h/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-

.PHONY: clean

obj-m := hx711.o

default:
	$(MAKE) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
