KERNELDIR := /home/brojackie/linux/IMX6ULL/linux-nxp/linux-imx-rel_imx_4.1.15_2.1.0_ga

CURRECT_PATH := $(shell pwd)

obj-m = spinlock.o

build:kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRECT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRECT_PATH) clean