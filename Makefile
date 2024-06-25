TARGET_MODULE := sort_test
obj-m :=$(TARGET_MODULE).o
sort_test-objs := \
	listsort.o \
	xoroshiro128p.o \
	sort_test_impl.o \
	sort_test_kernel.o

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

# CC = gcc
# CFLAGS = -O2

# all: main

# mainOBJS := main.o list_sort.o shiverssort.o \
#         timsort.o list_sort_old.o inplace_timsort.o
# measureOBJS := measure.o list_sort.o shiverssort.o \
# 		timsort.o list_sort_old.o inplace_timsort.o
		
# TARGET_MODULE := sort_test_kernel
# obj-m :=$(TARGET_MODULE).o

# deps := $(OBJS:%.o=.%.o.d)

# main: $(mainOBJS)
# 	$(CC) -o $@ $(LDFLAGS) $^
# 	$(MAKE) -C $(KDIR) M=$(PWD) modules
# 	gcc client.c -o client

# %.o: %.c
# 	$(CC) -o $@ $(CFLAGS) -c -MMD -MF .$@.d $<

# test: main
# 	@./main

# measure: $(measureOBJS)
# 	$(MAKE) main
# 	$(CC) -o $@ $(CFLAGS) $^

# load:
# 	sudo insmod $(TARGET_MODULE).ko

# unload:
# 	sudo rmmod $(TARGET_MODULE) || true >/dev/null

# plot:
# 	gnuplot plot.gp

# check: all
# 	$(MAKE) unload
# 	$(MAKE) load
# 	sudo ./client
# 	$(MAKE) unload

# clean:
# 	rm -f $(mainOBJS) $(deps) *~ main
# 	rm -f $(measureOBJS) $(deps) *~ main
# 	rm -rf *.dSYM

# -include $(deps)

#----------------------------------------------------------------------
### The following is the makefile that make the module test 
### The code is from other projects, need to me maintained

# TARGET_MODULE := sort_test
# obj-m :=$(TARGET_MODULE).o
# sort_test-objs := \
# 	heap.o \
# 	xoroshiro128plus.o \
# 	intro.o \
# 	test.o

# KDIR := /lib/modules/$(shell uname -r)/build
# PWD := $(shell pwd)
# all:
# 	$(MAKE) -C $(KDIR) M=$(PWD) modules
# 	gcc client.c -o client

# clean:
# 	rm -rf *.o *.ko *.mod.* *.symvers *.order *.mod.cmd *.mod
# 	$(RM) client out

# load:
# 	sudo insmod $(TARGET_MODULE).ko

# unload:
# 	sudo rmmod $(TARGET_MODULE) || true >/dev/null

# plot:
# 	gnuplot plot.gp

# check: all
# 	$(MAKE) unload
# 	$(MAKE) load
# 	sudo ./client
# 	$(MAKE) unload