KERNELDIR := /home/brojackie/linux/IMX6ULL/linux-nxp/linux-imx-rel_imx_4.1.15_2.1.0_ga

CURRENT_PATH := $(shell pwd)
obj-m := chrdevbase.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules	#实际调用内核的kBuild系统来编译外部模块
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean