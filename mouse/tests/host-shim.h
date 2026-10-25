#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#define VMIN 0
#define VTIME 1
#define TCSETS 1
#define TCSETSW 2
#define TCSETSF 3
#define INT_GENERIC 0
struct ch_queue { int len,size,head,tail; unsigned char *base; int wait; };
struct tty { struct ch_queue inq; struct { unsigned c_iflag,c_oflag,c_lflag; unsigned char c_cc[2]; } termios; int usecount; };
struct pt_regs { int unused; };
struct tty_ops { int (*open)(struct tty*); void (*release)(struct tty*); int (*write)(struct tty*); void *read; int (*ioctl)(struct tty*,int,char*); void *out; };
static struct { int euid; } task;
#define current (&task)
static unsigned char memory[65536];
static unsigned long vectors[256];
#define PMMEM memory
#define IVT vectors
static int irq_enabled, irq_busy, alloc_fail, irq_calls, command_mode, firmware_enabled;
static unsigned iflag=1, command, port_writes, ack_writes;
static unsigned char low, high, status, testport;
static int race_at_ack, race_in_queue;
#define save_flags(f) ((f)=iflag)
#define clr_irq() (iflag=0)
#define restore_flags(f) (iflag=(f))
static unsigned char inb(unsigned port) {
    if (port==0x2a3) return testport++;
    if (port==0x2a0) return status;
    if (port==0x2a1) return low;
    if (port==0x21) return irq_enabled ? 0x7f : 0xff;
    assert(0); return 0;
}
static void outb(unsigned value,unsigned port) {
    assert(!iflag); ++port_writes;
    if(port==0x2a1){low=value;return;}
    if(port==0x2a2){high=value;return;}
    assert(port==0x2a0);
    if(value==1){
        ++ack_writes;
        if(race_at_ack){memory[0x4359]=2;memory[0x435e]=1;race_at_ack=0;}
        memory[0x435f]=0;return;
    }
    assert(value==0x52||value==0x53); command=value;
    if(command_mode==1){status=1;return;}
    if(command_mode==2){status=2;return;}
    if(command_mode==3)return; /* ignored; low sentinel remains */
    firmware_enabled=value==0x52;low=0;status=0;
}
static int request_irq(int irq,void(*fn)(int,struct pt_regs*),int flags) {
    (void)fn;(void)flags;assert(irq==7);++irq_calls;
    if(irq_busy)return -EBUSY;
    irq_enabled=1;vectors[15]=0x12345678;return 0;
}
static void enable_irq(int irq){assert(irq==7);irq_enabled=1;}
static void disable_irq(int irq){assert(irq==7);irq_enabled=0;}
static void free_irq(int irq){disable_irq(irq);}
static void int_vector_set(unsigned n,unsigned off,unsigned seg){vectors[n]=((unsigned long)(seg&65535)<<16)|(off&65535);}
static int tty_allocq(struct tty*t,int n,int out) {
    (void)out;if(alloc_fail)return -ENOMEM;
    t->inq.base=malloc(n);t->inq.size=n;t->inq.len=t->inq.head=t->inq.tail=0;return 0;
}
static void tty_freeq(struct tty*t){free(t->inq.base);t->inq.base=NULL;}
static void test_queue_hook(void){
    assert(!iflag);
    if(race_in_queue){memory[0x4359]=2;memory[0x435e]=1;--race_in_queue;}
}
static void wake_up(int*w){(void)w;}
static int verified_memcpy_tofs(char*dst,const void*src,unsigned n){memcpy(dst,src,n);return 0;}
#define printk(...) ((void)0)
