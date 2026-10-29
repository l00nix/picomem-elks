#define PICOSTACK_TEST
#include "../picostack.c"
int main(void)
{
 unsigned j;unsigned char b[52]={0};
 memset(&task,0x7a,sizeof(task));task.pid=23;task.t_regs.orig_ax=38;
 test_sp=(uintptr_t)task.t_kstack+660;picostack_begin();
 assert(interrupts==1&&task.guard==0x7a7a&&task.upper==0x7a7a);
 for(j=0;j<322;j++)assert(task.t_kstack[j]==0x5555);
 for(;j<350;j++)assert(task.t_kstack[j]==0x7a7a);
 task.t_kstack[90]=0x1234;picostack_end();assert(peak==520&&peak_pid==23&&peak_call==38);
 picostack_read(b,0);assert(b[32]+(b[33]<<8)==520&&b[34]+(b[35]<<8)==700&&b[44]==1);
 test_sp=(uintptr_t)task.t_kstack+8;picostack_begin();assert(invalid==1&&task.guard==0x7a7a);
 test_sp=(uintptr_t)task.t_kstack+702;picostack_begin();assert(invalid==2);
 picostack_read(b,1);assert(!peak&&!samples&&!invalid&&interrupts==1);
 puts("PASS bounded stack paint, active-frame/guard preservation, high-water and reset");return 0;
}
