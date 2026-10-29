#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define SP_TYPE uintptr_t
#define FARPROC
#define KSTACK_BYTES 700
struct task_mock {unsigned short guard,t_kstack[350],upper;unsigned pid;struct {unsigned orig_ax;}t_regs;};
static struct task_mock task;
static struct task_mock *current=&task;
static uintptr_t test_sp;
static unsigned interrupts=1;
#define save_flags(f) ((f)=interrupts)
#define clr_irq() (interrupts=0)
#define restore_flags(f) (interrupts=(f))
