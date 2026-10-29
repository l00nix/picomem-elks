#define CONFIG_FS_PICOMEM_DIAG
#define PICODIAG_TEST
static struct {unsigned euid;} mock_task;
#define current (&mock_task)
static void respond_diag(void);
#define main base_tests
#include "test-picofs.c"
#undef main
static unsigned diag_fault,stack_resets;
void picostack_read(unsigned char *p,int reset)
{p[32]=200;p[34]=188;p[35]=2;p[42]=1;if(reset)stack_resets++;}
static void respond_diag(void)
{
 unsigned char *p=mmio+BASE;unsigned op=p[65],selector=p[66];
 assert(rd16(p+52)==68&&p[58]==20&&!memcmp(p+60,"PMDG",4));
 memset(p+60,0,52);memcpy(p+60,"PMDG",4);p[64]=1;p[65]=op;p[66]=selector;
 le16(p+84,2400);le16(p+86,4096);le16(p+52,112);le16(p+58,0);
 if(diag_fault)p[111]=1;
}
int main(void)
{
 unsigned char q[52]={0};struct pm_volume v={0};struct super_block sb={0};struct inode i={0};unsigned old;
 reset();v.drive=20;sb.u.generic_sbp=&v;i.i_sb=&sb;i.i_ino=1;
 memcpy(q,"PMDG",4);q[4]=1;q[6]=255;
 old=calls;assert(diag_inner(&i,0x5044,(char*)q)==-ENODEV&&calls==old&&!busy);
 memcpy(mmio+0x3fb0,"PMDG\1\x73\10\64l00nix\0\0",16);
 assert(!diag_inner(&i,0x5044,(char*)q)&&q[32]==200&&q[42]==1&&!busy&&irq_enabled);
 mock_task.euid=1;old=calls;assert(diag_inner(&i,0x5044,(char*)q)==-EPERM&&old==calls);mock_task.euid=0;
 q[5]=1;q[6]=9;q[7]=0;assert(diag_inner(&i,0x5044,(char*)q)==-EINVAL);
 q[6]=255;assert(!diag_inner(&i,0x5044,(char*)q)&&stack_resets==1);
 v.drive=18;assert(diag_inner(&i,0x5044,(char*)q)==-EINVAL);v.drive=20;
 i.i_ino=2;assert(diag_inner(&i,0x5044,(char*)q)==-EINVAL);i.i_ino=1;
 copy_fault=1;assert(diag_inner(&i,0x5044,(char*)q)==-EFAULT&&!busy);copy_fault=0;
 diag_fault=1;assert(diag_inner(&i,0x5044,(char*)q)==-EIO&&broken&&!busy);
 puts("PASS diagnostics exact marker, privilege, request/response, SD/root, copy-fault and lock guards");return 0;
}
