ifneq ($(KERNELRELEASE),)
obj-m := vcan.o
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	@rm -rf .tmp* .vcan* Module* modules* vcan.*o vcan.mod*
endif
