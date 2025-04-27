CFLAGS := -mcmodel=large -fno-builtin -m64 -std=gnu99

PIC := iAPIC


all: system
	objcopy -I elf64-x86-64 -S -R ".eh_frame" -R ".comment" -O binary system kernel.bin

system:	head.o main.o print_kernel.o interrupt.o interrupt_entry.o string.o debug.o memory.o bitmap.o list.o pic.o keyboard.o mouse.o \
	disk.o timer.o kernel_task.o thread.o
	ld -b elf64-x86-64 -o system head.o main.o interrupt.o interrupt_entry.o print_kernel.o string.o debug.o memory.o bitmap.o list.o \
	pic.o keyboard.o mouse.o disk.o timer.o kernel_task.o thread.o \
	-T kernel.lds 

main.o:	main.c
	gcc $(CFLAGS) -c main.c -D$(PIC)

interrupt.o: interrupt.c
	gcc $(CFLAGS) -c interrupt.c

print_kernel.o: print_kernel.c
	gcc -static $(CFLAGS) -c print_kernel.c -fno-stack-protector

string.o: lib/string.c
	gcc $(CFLAGS) -c lib/string.c

# 这里真的很奇怪，为什么debug.o编译不出来，只有在把上面的命令都注释完之后才能够编译出debug.o
# 我找出了原因，是因为我在system的依赖文件中没有写需要依赖debug.o
debug.o: lib/debug.c
	gcc $(CFLAGS) -c lib/debug.c

bitmap.o: lib/bitmap.c
	gcc $(CFLAGS) -c lib/bitmap.c

list.o: lib/list.c
	gcc $(CFLAGS) -c lib/list.c

ifeq ($(PIC),APIC)
pic.o: APIC.c
	gcc  $(CFLAGS) -c APIC.c -o pic.o
else
pic.o: 8259A.c
	gcc $(CFLAGS) -c 8259A.c -o pic.o
endif

keyboard.o: device/keyboard.c
	gcc $(CFLAGS) -c device/keyboard.c

mouse.o: device/mouse.c
	gcc $(CFLAGS) -c device/mouse.c

disk.o: device/disk.c
	gcc $(CFLAGS) -c device/disk.c

timer.o: device/timer.c
	gcc $(CFLAGS) -c device/timer.c

kernel_task.o: thread/kernel_task.c
	gcc $(CFLAGS) -c thread/kernel_task.c

thread.o: thread/thread.c
	gcc $(CFLAGS) -c thread/thread.c

memory.o: memory.c
	gcc $(CFLAGS) -c memory.c

head.o:	head.S
	gcc -E  head.S > head.s
	as --64 -o head.o head.s

interrupt_entry.o: interrupt_entry.asm
	nasm -f elf64 interrupt_entry.asm -o interrupt_entry.o

clean:
	rm -rf *.o *.s~ *.s *.S~ *.c~ *.h~ system  Makefile~ Kernel.lds~ kernel.bin 

