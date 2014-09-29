KERNELDIR := /lib/modules/3.12.28-2-ARCH/build 
PWD := $(shell pwd)

.PHONY: clean

obj-m := hx711.o

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
