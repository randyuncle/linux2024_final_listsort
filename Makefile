TARGET_MODULE := sort_test
obj-m :=$(TARGET_MODULE).o
sort_test-objs := \
	listsort.o \
	xoroshiro128p.o \
	sort_test_impl.o \
	sort_test_kernel.o \
	timsort_merge.o \
	timsort_linear.o \
	timsort_binary.o \
	timsort_b_gallop.o \
	timsort_l_gallop.o \
	shiverssort.o \
	shiverssort_merge.o \

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc client.c -o client

clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order *.mod.cmd *.mod
	$(RM) client out

load:
	sudo insmod $(TARGET_MODULE).ko

unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

plot:
	gnuplot plot.gp

check: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./client
	$(MAKE) unload