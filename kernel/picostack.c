/* SPDX-License-Identifier: GPL-2.0-or-later
 * Optional syscall-stack watermarks, including instrumentation overhead.
 * Leaf painter uses the actual SP, never a fixed assumed call depth.
 */
#ifdef PICOSTACK_TEST
#include "tests/stack-shim.h"
#else
#include <linuxmt/config.h>
#include <linuxmt/sched.h>
#include <linuxmt/kernel.h>
#include <arch/irq.h>
#ifndef CONFIG_FARTEXT_KERNEL
#error PicoMEM diagnostics require CONFIG_FARTEXT_KERNEL
#endif
#define SP_TYPE unsigned
#endif
static unsigned peak,peak_pid,peak_call,invalid;
static unsigned long samples;
void FARPROC picostack_begin(void)
{
 unsigned flags;SP_TYPE sp,lo=(SP_TYPE)current->t_kstack,hi=lo+KSTACK_BYTES;
 unsigned short *p=current->t_kstack;
 save_flags(flags);clr_irq();
#ifdef PICOSTACK_TEST
 sp=test_sp;
#else
 __asm__ volatile("mov %%sp,%0":"=r"(sp)::"memory");
#endif
 if(sp<lo+16||sp>hi){if(invalid!=65535)invalid++;restore_flags(flags);return;}
 sp=(sp-16)&~(SP_TYPE)1;
 while((SP_TYPE)p<sp)*p++=0x5555;
 restore_flags(flags);
}
void FARPROC picostack_end(void)
{
 unsigned j=0,n;
 while(j<KSTACK_BYTES/2&&current->t_kstack[j]==0x5555)j++;
 n=KSTACK_BYTES-j*2;
 if(n>peak){peak=n;peak_pid=current->pid;peak_call=current->t_regs.orig_ax;}
 if(samples!=0xffffffffUL)samples++;
}
void FARPROC picostack_read(unsigned char *p,int reset)
{
 unsigned flags;save_flags(flags);clr_irq();
 if(reset){peak=peak_pid=peak_call=invalid=0;samples=0;}
#define PUT(n,x) do{p[n]=(unsigned char)(x);p[(n)+1]=(unsigned char)((x)>>8);}while(0)
 PUT(32,peak);PUT(34,KSTACK_BYTES);PUT(36,peak_pid);PUT(38,peak_call);
 PUT(40,invalid);PUT(42,1);PUT(44,(unsigned)samples);PUT(46,(unsigned)(samples>>16));
#undef PUT
 restore_flags(flags);
}
