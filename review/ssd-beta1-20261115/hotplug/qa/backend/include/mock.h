#ifndef MOCK_H
#define MOCK_H
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#define CONFIG_ARCH_IBMPC 1
#define CONFIG_BLK_DEV_SSD_PICOMEM 1
static unsigned release_warnings;
#define printk(message) (assert(strcmp((message),"ssd: USB not released; power cycle required\n")==0), ++release_warnings)
typedef uint32_t sector_t;
typedef unsigned ramdesc_t;
typedef unsigned jiff_t;
struct inode { int unused; }; struct file { int unused; };
extern unsigned kernel_ds;
static unsigned irq_enabled=1, copies_to_caller, copies_from_caller;
#define save_flags(f) ((f)=irq_enabled)
#define clr_irq() (irq_enabled=0)
#define restore_flags(f) (irq_enabled=(f))
static unsigned char inb(unsigned);
static void outb(unsigned char,unsigned);
static void xms_fmemcpyw(void *d,unsigned ds,const void *s,unsigned ss,unsigned n) {
 assert(!irq_enabled && n==256);
 if(ds==0x1234 && ss==kernel_ds) ++copies_to_caller;
 else { assert(ds==kernel_ds && ss==0x1234); ++copies_from_caller; }
 memcpy(d,s,n*2);
}
#endif
