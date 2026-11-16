from pathlib import Path
import subprocess,json,hashlib
p=Path(__file__).resolve().parent
src=p.parent/'firmware/src/isa_devices/dev_picomem_io.cpp'
s=src.read_text(); body='\n'.join(x for x in s.splitlines() if not x.lstrip().startswith('#include'))
prefix=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define DBG_PIN_AUDIO 0
#define STAT_READY 0
#define STAT_CMDINPROGRESS 1
#define STAT_DATA_READ 0x10
#define STAT_DATA_WRITE 0x11
#define PORT_PM_CMD_STATUS 0
#define PORT_PM_CMD_DATAL 1
#define PORT_PM_CMD_DATAH 2
#define PORT_TEST 3
#define PORT_TR_L 4
#define CMD_LockPort 0x99
#define CMD_PMIRQ_ACK 0x0f
#define CMD_DMACPY_ACK 0x10
static volatile uint8_t PM_Status,PM_CmdDataL,PM_CmdDataH,PM_Command;
static volatile bool PM_CmdReset;
struct spin_lock_t { bool locked; };
static spin_lock_t only_lock;
static int spin_lock_claim_unused(bool) { return 0; }
static spin_lock_t *spin_lock_instance(int) { return &only_lock; }
static uint32_t spin_lock_blocking(spin_lock_t *p) { assert(!p->locked);p->locked=true;return 1; }
static void spin_unlock(spin_lock_t *p,uint32_t) { assert(p->locked);p->locked=false; }
static unsigned ticks, mode, copied;
static uint8_t wire[516];
static uint32_t time_us_32() { ticks += mode == 2 ? 100000 : 1; return ticks; }
static void tight_loop_contents();
static void multicore_fifo_push_blocking(int) {}
static void bus_pm_irq_ack() {}
static void isa_dmacpy_ack() {}
bool dev_pmio_do_rw(uint8_t *,uint16_t,bool,bool);
'''
suffix=r'''
static void tight_loop_contents() {
 assert(!only_lock.locked);
 if (mode == 2) return;
 if (mode == 3) { PM_CmdReset=true;return; }
 if (PM_Status == STAT_DATA_READ) {
  assert(copied < sizeof(wire)); assert(dev_pmio_ior(4,&wire[copied++]));
 } else if (PM_Status == STAT_DATA_WRITE) {
  assert(copied < sizeof(wire));dev_pmio_iow(4,wire[copied++]);
 }
}
static void reset(unsigned which=0) {
 assert(!only_lock.locked);mode=which;ticks=copied=0;PM_CmdReset=false;
 PM_TR_CNT_L=0;PM_TR_CNT_H=37;PM_Status=STAT_READY;
}
int main() {
 unsigned checks=0;uint8_t data[514];data[0]=0x6a;data[513]=0xb6;
 for(unsigned i=0;i<512;++i)data[i+1]=i^0x79;
 reset();assert(dev_pmio_do_rw(data+1,512,true,false));
 assert(copied==512 && !memcmp(wire,data+1,512) && !PM_TR_CNT_L && !PM_TR_CNT_H && !PM_TR_PTR);++checks;
 reset();memset(data+1,0,512);assert(dev_pmio_do_rw(data+1,512,false,false));
 assert(!memcmp(wire,data+1,512) && data[0]==0x6a && data[513]==0xb6);++checks;
 reset();assert(dev_pmio_do_rw(data+1,8,true,false)&&copied==8);++checks;
 reset(2);assert(!dev_pmio_do_rw(data+1,512,false,false));
 assert(!PM_TR_PTR&&!PM_TR_CNT_L&&!PM_TR_CNT_H);++checks;
 uint8_t old=data[1];dev_pmio_iow(4,0xff);assert(data[1]==old);++checks;
 uint8_t v=0;assert(!dev_pmio_ior(4,&v)&&v==0xff);++checks;
 reset(3);assert(!dev_pmio_do_rw(data+1,512,true,false)&&!PM_TR_PTR&&!PM_TR_CNT_L&&!PM_TR_CNT_H);++checks;
 reset();assert(!dev_pmio_do_rw(data+1,512,true,true));++checks;
 reset();assert(!dev_pmio_do_rw(data+1,0,true,false));++checks;
 reset();ticks=0xfffffff0;assert(dev_pmio_do_rw(data+1,8,true,false));++checks;
 uint8_t packet[518];packet[0]=0xa5;packet[517]=0x5a;
 for(unsigned i=0;i<516;i++)wire[i]=i^0x93;
 reset();assert(dev_pmio_do_rw(packet+1,516,false,false));
 assert(copied==516&&!memcmp(packet+1,wire,516)&&packet[0]==0xa5&&packet[517]==0x5a&&!PM_TR_PTR&&!PM_TR_CNT_L&&!PM_TR_CNT_H);++checks;
 printf("PIO acceptance: %u checks passed\n",checks);
}
'''
(p/'generated.cpp').write_text(prefix+body+suffix)
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined',str(p/'generated.cpp'),'-o',str(p/'acceptance')],check=True)
r=subprocess.run([str(p/'acceptance')],check=True,capture_output=True,text=True);print(r.stdout)
(p/'result.json').write_text(json.dumps({'passed':True,'checks':11,'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'scope':'Production translation unit with includes replaced by mocks; cooperative core scheduling, not a hardware timing proof','output':r.stdout},indent=2)+'\n')
