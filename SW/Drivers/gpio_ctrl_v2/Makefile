
DRV_NAME := gpio_ctrl

DEV_MAJOR := 270

TARGET := $(DRV_NAME).ko

MDIR := drivers/pwm

CURRENT := $(shell uname -r)
KDIR ?= /lib/modules/$(CURRENT)/build
PWD := $(shell pwd)
DEST := /lib/modules/$(CURRENT)/kernel/$(MDIR)
EXTRA_CFLAGS := -I$(PWD) -DDEV_MAJOR=$(DEV_MAJOR)

obj-m := gpio_ctrl.o
gpio_ctrl-objs := main.o

.PHONY: default
default: build

.PHONY: prerequisites
prerequisites:
	sudo apt update
	sudo apt -y install linux-headers-$(uname -r)

.PHONY: setup
setup:
	dtc -@ -I dts -O dtb -o gpio_ctrl.dtbo gpio_ctrl_overlay.dts
	sudo cp gpio_ctrl.dtbo /boot/firmware/overlays/
	echo 'dtoverlay=gpio_ctrl' | sudo tee -a /boot/firmware/config.txt
	sudo sed -i 's/dtparam=i2c_arm=on/dtparam=i2c_arm=off/g' /boot/firmware/config.txt

.PHONY: build
build: stop check_is_major_free
	$(MAKE) -C $(KDIR) M=$(PWD)

.PHONY: check_is_major_free
check_is_major_free:
	if [ `cat /proc/devices | grep -c $(DEV_MAJOR)` != 0 ]; \
	then \
		echo "Major $(DEV_MAJOR) is occupied!"; \
		exit 1; \
	fi
	if [ `cat /proc/devices | grep -c $(DEV_MAJOR)` != 0 ]; \
	then \
		echo "Major $(DEV_MAJOR) is occupied!"; \
		exit 1; \
	fi

.PHONY: start
start:
	sudo insmod $(TARGET)
	sudo chmod a+rw /dev/gpio_stream
	
.PHONY: stop
stop:
	if [ `lsmod | grep -c $(DRV_NAME)` != 0 ]; \
	then \
		sudo rmmod $(TARGET:%.ko=%); \
	fi

.PHONY: install
install:
	sudo cp $(TARGET) $(DEST)
	sudo depmod -a

.PHONY: clean
clean:
	rm -f *.o $(TARGET) .*.cmd .*.flags *.mod.c *. \
		modules.order Module.symvers *.mod \
		*.dtbo

-include $(KDIR)/Rules.make
