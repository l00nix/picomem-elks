/* Adapted from the retained read-only acceptance.c. Include current build source
 * directly; do not copy, substitute or reimplement backend functions. */
#include <stdio.h>
#include "mock.h"
#include PM_BACKEND_SOURCE
unsigned kernel_ds=0x1000;
sector_t ssd_num_sects;
char ssd_initialized;
static unsigned char phase,tick,operation,info[8],payload[512],received[516];
static unsigned pos,transactions,reads,writes,resets,polls,data_in,data_out;
static unsigned count_value,stage_number;
static int bad_counter,reset_stalls;
/* Fault stage: 1 first handshake; 2 read payload handshake; 3 final status. */
static unsigned fault_stage,fault_kind;
enum {NONE, PENDING, WRONG_STAGE, LOW_COUNT, HIGH_COUNT, ERROR2, ERROR3, ABSENT};
static unsigned checks;
static void stage(unsigned number,unsigned wanted,unsigned count) {
 stage_number=number;phase=wanted;count_value=count;pos=0;
 if(number!=fault_stage)return;
 switch(fault_kind){
 case PENDING:phase=1;break;
 case WRONG_STAGE:phase=wanted==PM_READ?PM_WRITE:PM_READ;break;
 case LOW_COUNT:count_value^=1;break;
 case HIGH_COUNT:count_value^=256;break;
 case ERROR2:phase=2;break;
 case ERROR3:phase=3;break;
 case ABSENT:phase=0xff;break;
 default:break;
 }
}
static unsigned char inb(unsigned port) {
 assert(!irq_enabled);++reads;
 switch(port-PM_PORT){
 case 0:++polls;return phase;
 case 1:return count_value&255;
 case 2:return count_value>>8;
 case 3:return bad_counter?0xff:tick++;
 case 4:{
  assert(phase==PM_READ);
  unsigned char x;
  if(operation==0){assert(pos<8);x=info[pos++];if(pos==8)stage(3,0,0);}
  else {assert(operation==1&&pos<512);x=payload[pos++];if(pos==512)stage(3,0,0);}
  ++data_in;return x;
 }
 default:assert(0);return 0;
 }
}
static void outb(unsigned char value,unsigned port) {
 assert(!irq_enabled);++writes;
 switch(port-PM_PORT){
 case 0:
  if(value==0){++resets;phase=reset_stalls?1:0;break;}
  assert(value==PM_COMMAND);++transactions;
  stage(1,operation==0?PM_READ:PM_WRITE,operation==0?8:operation==1?4:516);break;
 case 1:operation=value;break;
 case 2:assert(value==0);break;
 case 4:
  assert(phase==PM_WRITE);++data_out;
  if(operation==1){assert(pos<4);received[pos++]=value;if(pos==4)stage(2,PM_READ,512);}
  else {assert(operation==2&&pos<516);received[pos++]=value;if(pos==516)stage(3,0,0);}
  break;
 default:assert(0);
 }
}
static void reset_mock(unsigned irq){
 phase=tick=operation=0;pos=transactions=reads=writes=resets=polls=data_in=data_out=0;
 count_value=stage_number=fault_stage=fault_kind=0;bad_counter=reset_stalls=0;
 copies_to_caller=copies_from_caller=0;failed=ssd_initialized=0;ssd_num_sects=0;irq_enabled=irq;
 memcpy(info,"PM\x00\x00\x02\x00\x00\x02",8);
 memset(received,0xcc,sizeof received);
 for(unsigned i=0;i<512;i++)payload[i]=(unsigned char)(i^0xa5);
}
static void healthy(unsigned irq){
 reset_mock(irq);ssd_num_sects=ssddev_init();
 assert(ssd_num_sects==131072&&ssd_initialized&&!failed&&irq_enabled==irq);
 assert(transactions==1&&data_in==8&&resets==0);
}
static void passed(const char *label,unsigned a,unsigned b,unsigned c){
 printf("PASS %s %u %u %u\n",label,a,b,c);++checks;
}
static void unchanged(const char *buf){for(unsigned i=0;i<512;i++)assert((unsigned char)buf[i]==0x77);}
static void no_reuse(char *buf,unsigned irq){
 unsigned r=reads,w=writes,ct=copies_to_caller,cf=copies_from_caller;
 fault_kind=fault_stage=0;phase=0;
 assert(!ssddev_read(0,buf,0x1234));assert(!ssddev_write(0,buf,0x1234));
 assert(reads==r&&writes==w&&copies_to_caller==ct&&copies_from_caller==cf&&irq_enabled==irq);
}
int main(void){
 char buf[512];
 for(unsigned irq=0;irq<=1;irq++){
  healthy(irq);passed("init",irq,0,0);
  memset(buf,0x77,512);
  assert(ssddev_read(0x12345,buf,0x1234)&&irq_enabled==irq&&!failed);
  assert(!memcmp(buf,payload,512)&&received[0]==0x45&&received[1]==0x23&&received[2]==1&&received[3]==0);
  assert(copies_to_caller==1&&copies_from_caller==0&&data_out==4&&data_in==520&&resets==0);
  passed("read-success",irq,0,0);
  for(unsigned i=0;i<512;i++)buf[i]=(char)(i^0x3c);
  assert(ssddev_write(0x12345,buf,0x1234)&&irq_enabled==irq&&!failed);
  assert(received[0]==0x45&&received[1]==0x23&&received[2]==1&&received[3]==0&&!memcmp(received+4,buf,512));
  assert(copies_from_caller==1&&data_out==520&&resets==0);passed("write-success",irq,0,0);
  for(unsigned op=1;op<=2;op++)for(unsigned edge=0;edge<2;edge++){
   unsigned r=reads,w=writes,ct=copies_to_caller,cf=copies_from_caller;
   sector_t lba=edge?UINT32_MAX:ssd_num_sects;
   assert(!(op==1?ssddev_read(lba,buf,0x1234):ssddev_write(lba,buf,0x1234)));
   assert(reads==r&&writes==w&&copies_to_caller==ct&&copies_from_caller==cf&&!failed&&irq_enabled==irq);
   passed("range-no-io",irq,op,edge);
  }
  for(unsigned op=1;op<=2;op++)for(unsigned st=1;st<=3;st++){
   if(op==2&&st==2)continue;
   for(unsigned kind=PENDING;kind<=ABSENT;kind++){
    if(st==3&&(kind==LOW_COUNT||kind==HIGH_COUNT))continue;
    healthy(irq);fault_stage=st;fault_kind=kind;memset(buf,0x77,512);
    unsigned start_polls=polls;
    assert(!(op==1?ssddev_read(0x12345,buf,0x1234):ssddev_write(0x12345,buf,0x1234)));
    assert(failed&&irq_enabled==irq);unchanged(buf);
    assert(copies_to_caller==0&&copies_from_caller==(op==2));
    assert(resets==(kind==ERROR2||kind==ERROR3));
    if(kind==PENDING||kind==WRONG_STAGE)assert(polls-start_polls>=300000&&polls-start_polls<300010);
    else assert(polls-start_polls<20);
    if(st==1)assert(data_out==0&&data_in==8);
    if(st==2)assert(data_out==4&&data_in==8);
    if(st==3)assert(op==1?(data_out==4&&data_in==520):(data_out==516&&data_in==8));
    no_reuse(buf,irq);passed("transfer-fault",irq,op,st*10+kind);
   }
  }
  /* A completed error may be cleared once; failure of that clear stays bounded. */
  for(unsigned op=1;op<=2;op++){
   healthy(irq);fault_stage=3;fault_kind=ERROR2;reset_stalls=1;memset(buf,0x77,512);
   unsigned start_polls=polls;
   assert(!(op==1?ssddev_read(0,buf,0x1234):ssddev_write(0,buf,0x1234)));
   assert(failed&&resets==1&&phase==1&&irq_enabled==irq&&polls-start_polls>=300000&&polls-start_polls<300010);
   unchanged(buf);no_reuse(buf,irq);passed("clear-timeout",irq,op,0);
  }
  /* Pre-existing bus status is never reset: the backend did not issue it. */
  unsigned statuses[]={1,2,3,0xff};
  for(unsigned op=0;op<=2;op++)for(unsigned i=0;i<4;i++){
   if(op)healthy(irq);else reset_mock(irq);
   phase=statuses[i];unsigned t=transactions;memset(buf,0x77,512);
   assert(!(op==0?ssddev_init():op==1?ssddev_read(0,buf,0x1234):ssddev_write(0,buf,0x1234)));
   assert(failed&&transactions==t&&resets==0&&phase==statuses[i]&&irq_enabled==irq);unchanged(buf);
   no_reuse(buf,irq);passed("preexisting-busy-error",irq,op,statuses[i]);
  }
  for(unsigned st=1;st<=3;st+=2)for(unsigned kind=PENDING;kind<=ABSENT;kind++){
   if(st==3&&(kind==LOW_COUNT||kind==HIGH_COUNT))continue;
   reset_mock(irq);fault_stage=st;fault_kind=kind;
   assert(!ssddev_init()&&failed&&!ssd_initialized&&irq_enabled==irq);
   assert(transactions==1&&resets==(kind==ERROR2||kind==ERROR3));
   if(kind==PENDING||kind==WRONG_STAGE)assert(polls>=300000&&polls<300010);
   else assert(polls<20);
   assert(data_in==(st==3?8:0));no_reuse(buf,irq);
   passed("init-issued-fault",irq,st,kind);
  }
  for(unsigned field=0;field<7;field++){
   reset_mock(irq);
   if(field==0)bad_counter=1;
   if(field==1)info[0]='X';if(field==2)info[1]='X';
   if(field==3)info[6]=1;if(field==4)info[7]=4;
   if(field==5)memset(info+2,0,4);
   if(field==6){fault_stage=1;fault_kind=LOW_COUNT;}
   assert(!ssddev_init()&&failed&&!ssd_initialized&&irq_enabled==irq);
   assert(transactions==(field!=0)&&resets==0);passed("init-invalid",irq,field,0);
  }
 }
 assert(ssddev_ioctl(0,0,0,0)==-EINVAL);passed("ioctl",0,0,0);
 printf("SUMMARY %u checks passed\n",checks);return 0;
}
