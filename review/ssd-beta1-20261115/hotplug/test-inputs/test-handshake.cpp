#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#define __force_inline inline
struct Pio {unsigned inte0=0,inte1=0;};static Pio block;static Pio *isa_pio=&block;
static const unsigned isa_bus_sm=0,isa_bus_DATA_READY_IRQ=4,pis_interrupt4=12;
static const unsigned DEV_PM=1,PORT_TR_L=4,PIN_IORDY=34;
static std::vector<unsigned> events;
static bool active_input,flag,data_queued,data_driven,selected,stopped;
static unsigned polls,event_at,clears,ready_releases,remaining;
struct Fatal {};
[[noreturn]] static void pm_payload_ready_fault(){stopped=true;events.push_back(9);throw Fatal();}
static bool pio_interrupt_get(Pio *p,unsigned bit){
 assert(p==isa_pio&&bit==4&&!stopped);polls++;
 assert(data_queued);
 if(polls==event_at){data_driven=true;flag=true;}
 return flag;
}
static void pio_interrupt_clear(Pio *p,unsigned bit){assert(p==isa_pio&&bit==4&&!stopped);flag=false;clears++;events.push_back(1);}
static void pio_sm_put(Pio *p,unsigned sm,uint32_t data){assert(p==isa_pio&&sm==0&&!stopped);(void)data;data_queued=true;events.push_back(3);}
static void gpioc_hi_out_clr(unsigned bits){assert(bits==4&&!stopped);if(selected)assert(data_driven&&flag);ready_releases++;events.push_back(4);}
static uint8_t pio_sm_get_blocking(Pio *p,unsigned sm){assert(p==isa_pio&&sm==0&&!stopped);events.push_back(5);return 0;}
static bool dev_pmio_ior(unsigned,uint8_t *data){*data=active_input?0x5d:0xff;if(active_input&&remaining)remaining--;return active_input;}
static void pm_do_ior(){if(selected)assert(!flag&&clears==1);events.push_back(2);}
#if PM_USB_SSD
#include "wait-ready.inc"
#endif
static void run(bool ISA_WasRead,unsigned IO_Device,unsigned ISA_IO_Addr){
 uint8_t ISA_Data=0x5d;
#if PM_USB_SSD
 bool PM_PayloadRead=false;
#endif
 if(ISA_WasRead)switch(IO_Device){
#include "pm-case.inc"
 default:break;
 }
#include "read-tail.inc"
}
static void reset(){events.clear();block={};active_input=true;flag=true;data_queued=data_driven=selected=stopped=false;polls=clears=ready_releases=0;event_at=1;remaining=1;}
int main(){unsigned tests=0;
 for(unsigned active=0;active<2;active++)for(unsigned read=0;read<2;read++)for(unsigned dev=0;dev<3;dev++)for(unsigned port=0;port<8;port++){
  reset();active_input=active;selected=PM_USB_SSD&&read&&active&&dev==DEV_PM&&port==4;
  run(read,dev,0x2a0+port);
  assert(!stopped);assert(clears==(selected?1:0));assert(polls==(selected?1:0));assert(ready_releases==read);
  if(selected){assert(events==std::vector<unsigned>({1,2,3,4,5}));assert(remaining==0);}
  tests++;
 }
#if PM_USB_SSD
 for(unsigned when:{1u,17u,4095u,4096u,4097u,0u}){
  reset();selected=true;event_at=when;
  try{run(true,DEV_PM,0x2a4);assert(when>0&&when<=4096);assert(ready_releases==1);}
  catch(const Fatal&){assert(when==0||when>4096);assert(ready_releases==0&&polls==4096);assert(events.back()==9);assert(!data_driven);}
  assert(clears==1);assert(polls<=4096);tests++;
 }
 for(unsigned route=0;route<2;route++){
  reset();selected=true;(route?block.inte1:block.inte0)=1u<<pis_interrupt4;
  try{run(true,DEV_PM,0x2a4);assert(false);}catch(const Fatal&){}
  assert(stopped&&clears==0&&polls==0&&ready_releases==0&&!data_queued);tests++;
 }
 reset();selected=true;block.inte0=1u<<7;run(true,DEV_PM,0x2a4);assert(!stopped&&ready_releases==1);tests++;
 // After missing event, a late event cannot run release/completion code.
 reset();selected=true;event_at=0;
 try{run(true,DEV_PM,0x2a4);assert(false);}catch(const Fatal&){}
 auto frozen=events;flag=true;data_driven=true;assert(stopped&&events==frozen&&ready_releases==0);tests++;
#endif
 printf("PASS %u handshake/scope cases PM_USB_SSD=%d; model does not simulate electrical timing\n",tests,PM_USB_SSD);
}
