# SPDX-License-Identifier: GPL-2.0
obj-m += elgato_gchd.o
elgato_gchd-objs := core.o fops.o usb.o transcoder.o input.o protocol_common.o app.o
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
