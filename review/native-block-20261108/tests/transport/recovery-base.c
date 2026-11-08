/* Uses the actual patched TinyUSB MSC state machine; no physical USB access. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef PMW_MSC_SOURCE
#define PMW_MSC_SOURCE "../pm-raw-publication-20261104/tinyusb-upstream/src/class/msc/msc_host.c"
#endif
#include PMW_MSC_SOURCE
#include "writer.c"
static struct { bool set; uint8_t ep,*buf; uint16_t len; } queued;
static uint32_t clock_us;
static unsigned checks, commands, writes, syncs, fault_op, fault_stage, inject;
static bool cache_on, reenter;
static void (*pump_hook)(void);
static void (*reply_hook)(unsigned, unsigned, uint8_t *, unsigned *);
static unsigned bad_mode;
static uint8_t media[100][512], cache[100][512], payload[512];
enum { NONE, REJECT, TIMEOUT, FAILED, STALL, SHORT, WRONG_EP, SIGNATURE,
       TAG, RESIDUE, STATUS_FAIL, STATUS_PHASE, DETACH, BAD_CBW, BAD_ARG };
uint32_t pmw_now_us(void) { return clock_us; }
bool usbh_edpt_claim(uint8_t d,uint8_t ep){(void)d;(void)ep;return true;}
bool usbh_edpt_release(uint8_t d,uint8_t ep){(void)d;(void)ep;return true;}
bool base_xfer(uint8_t d,uint8_t ep,uint8_t *b,uint16_t n,tuh_xfer_cb_t cb,uintptr_t u)
{
 (void)d;(void)cb;(void)u;
 msch_interface_t *m=get_itf(1); unsigned op=get_epbuf(1)->cbw.command[0];
 if(m->stage==MSC_STAGE_CMD) {commands++;if(op==0x2a)writes++;if(op==0x35)syncs++;}
 if(op==fault_op && m->stage==fault_stage && inject==REJECT)return false;
 assert(!queued.set); queued.set=true;queued.ep=ep;queued.buf=b;queued.len=n;return true;
}
void base_pump(void)
{
 clock_us+=10000;
 if(pump_hook){void (*hook)(void)=pump_hook;pump_hook=NULL;hook();}
 if(!queued.set)return;
 msch_interface_t *m=get_itf(1); msc_cbw_t *cbw=&get_epbuf(1)->cbw;
 unsigned op=cbw->command[0], stage=m->stage;
 unsigned bad=(op==fault_op && stage==fault_stage)?inject:NONE;
 if(bad==TIMEOUT)return;
 if(reenter) { reenter=false; assert(pmw_write(7,1,payload,512)==PMW_BUSY);
  assert(pmw_prepare(7)==PMW_BUSY);assert(pmw_flush(7)==PMW_BUSY);assert(pmw_release(7)==PMW_BUSY); }
 uint8_t ep=queued.ep,*buf=queued.buf; unsigned n=queued.len;
 queued.set=false;
 if(stage==MSC_STAGE_DATA) {
  if(op==0x25) {put32(buf,bad_mode==9?UINT32_MAX:99);put32(buf+4,bad_mode==10?4096:512);}
  else if(op==0x5a) {
   memset(buf,0,n);buf[1]=26;buf[8]=8;buf[9]=18;buf[10]=cache_on?4:0;n=28;
   switch(bad_mode) {
    case 1:n=7;break; case 2:buf[1]=63;break; case 3:buf[3]=0x80;break;
    case 4:buf[7]=64;break;case 5:buf[8]=0x48;break;case 6:buf[9]=17;break;
    case 7:buf[9]=30;break;case 8:buf[1]=0;break;
   }
  } else if(op==0x28) {
   uint32_t lba=be32(cbw->command+2);assert(lba<100 && n==512);memcpy(buf,media[lba],512);
  } else if(op==0x2a) {
   uint32_t lba=be32(cbw->command+2);assert(lba<100);assert(n==512);
   memcpy(cache[lba],buf,512);if(!cache_on)memcpy(media[lba],buf,512);
  }
 } else if(stage==MSC_STAGE_STATUS) {
  msc_csw_t *csw=(msc_csw_t*)buf;
  csw->signature=MSC_CSW_SIGNATURE;csw->tag=cbw->tag;
  csw->data_residue=cbw->total_bytes-m->data_xferred;csw->status=0;
  if(bad==SIGNATURE)csw->signature^=1;
  if(bad==TAG)csw->tag++;
  if(bad==RESIDUE)csw->data_residue++;
  if(bad==STATUS_FAIL)csw->status=1;
  if(bad==STATUS_PHASE)csw->status=2;
  if(op==0x35 && bad==NONE)memcpy(media,cache,sizeof(media));
 }
 if(reply_hook)reply_hook(op,stage,buf,&n);
 if(bad==DETACH)pmw_detach(1);
 if(bad==BAD_CBW)cbw->command[1]^=1;
 if(bad==BAD_ARG)m->complete_arg++;
 if(bad==SHORT)n=n? n-1:0;
 if(bad==WRONG_EP)ep=0x83;
 (void)msch_xfer_cb(1,ep,bad==FAILED?XFER_RESULT_FAILED:bad==STALL?XFER_RESULT_STALLED:XFER_RESULT_SUCCESS,n);
}
static void fresh(bool cached)
{
 /* Test-only restart: no outstanding physical transfer exists in this mock. */
 memset(&w,0,sizeof(w));msch_init();memset(&queued,0,sizeof(queued));
 msch_interface_t *m=get_itf(1);m->configured=m->mounted=true;m->max_lun=1;m->ep_in=0x81;m->ep_out=2;
 memset(media,0xa5,sizeof(media));memcpy(cache,media,sizeof(media));
 for(unsigned i=0;i<512;i++)payload[i]=(uint8_t)(i*17);
 pump_hook=NULL;reply_hook=NULL;clock_us=0;commands=writes=syncs=fault_op=fault_stage=inject=bad_mode=0;cache_on=cached;reenter=false;
 assert(pmw_attach(1,7)==PMW_OK);
}
static void ready(bool cached) {fresh(cached);assert(pmw_prepare(7)==PMW_OK);assert(!writes);}
static void blocked(void)
{
 unsigned before=commands;
 assert(pmw_write(7,0,payload,512)==PMW_FAULT);assert(pmw_flush(7)==PMW_FAULT);
 assert(pmw_release(7)==PMW_FAULT);assert(pmw_prepare(7)==PMW_FAULT);
 pmw_detach(1);assert(pmw_attach(1,8)==PMW_FAULT);assert(commands==before);
}
int main(void)
{
 for(unsigned c=0;c<2;c++) {
  ready(c);assert(commands==(c?3:2));reenter=true;
  assert(pmw_write(7,0,payload,512)==PMW_OK);assert(!memcmp(media[0],payload,512));
  assert(pmw_write(7,99,payload,512)==PMW_OK);assert(!memcmp(media[99],payload,512));
  assert(media[1][0]==0xa5);assert(writes==2 && syncs==(c?3:0));assert(!w.dirty);
  assert(pmw_flush(7)==PMW_OK);assert(pmw_release(7)==PMW_OK);
  unsigned before=commands;assert(pmw_write(7,0,payload,512)==PMW_DENIED);assert(commands==before);checks++;
 }
 ready(false);unsigned before=commands;
 assert(pmw_write(6,0,payload,512)==PMW_STALE);assert(pmw_write(7,100,payload,512)==PMW_DENIED);
 assert(pmw_write(7,0,NULL,512)==PMW_DENIED);
 for(unsigned n=0;n<514;n++)if(n!=512)assert(pmw_write(7,0,payload,n)==PMW_DENIED);
 assert(commands==before);checks++;
 for(bad_mode=1;bad_mode<=10;) {
  unsigned mode=bad_mode;fresh(true);bad_mode=mode;
  assert(pmw_prepare(7)==PMW_DENIED);assert(!writes && !w.prepared && !w.dirty);checks++;bad_mode=mode+1;
 }
 /* Failure at each WRITE and post-write FLUSH stage, including ambiguous writes. */
 for(unsigned opi=0;opi<2;opi++)
 for(unsigned stage=MSC_STAGE_CMD;stage<=MSC_STAGE_STATUS;stage++)
 for(unsigned how=REJECT;how<=BAD_ARG;how++) {
  unsigned op=opi?0x35:0x2a;
  if(opi && stage==MSC_STAGE_DATA)continue;
  if(how>=SIGNATURE && how<=STATUS_PHASE && stage!=MSC_STAGE_STATUS)continue;
  if((how==BAD_CBW || how==BAD_ARG) && stage!=MSC_STAGE_STATUS)continue;
  if(how==SHORT && opi && stage==MSC_STAGE_DATA)continue;
  ready(true);fault_op=op;fault_stage=stage;inject=how;
  int result=pmw_write(7,2,payload,512);
  if(result!=PMW_IO)fprintf(stderr,"unexpected op=%x stage=%u how=%u result=%d\n",op,stage,how,result);
  assert(result==PMW_IO);assert(w.fault && w.dirty);assert(writes==1);blocked();checks++;
 }
 /* Query failures must never queue WRITE. Clean SCSI failure denies preparation. */
 for(unsigned opi=0;opi<3;opi++)for(unsigned how=REJECT;how<=STATUS_PHASE;how++) {
  fresh(true);fault_op=opi==0?0x25:opi==1?0x5a:0x35;fault_stage=MSC_STAGE_STATUS;inject=how;
  int r=pmw_prepare(7);assert(r==((how==STATUS_FAIL || (opi==1 && how==RESIDUE))?PMW_DENIED:PMW_IO));
  assert(!writes && !w.prepared && !w.dirty);checks++;
 }
 /* Timeout leaves transfer storage private and permanently quarantined. */
 ready(true);fault_op=0x2a;fault_stage=MSC_STAGE_DATA;inject=TIMEOUT;
 uint8_t original[512];memcpy(original,payload,512);
 assert(pmw_write(7,3,payload,512)==PMW_IO);assert(queued.set && queued.buf==w.buffer);
 memset(queued.buf,0x42,queued.len);assert(!memcmp(payload,original,512));
 inject=NONE;while(queued.set)pmw_pump();assert(w.fault && w.dirty && !w.pending);blocked();checks++;
 /* Clock wrapping cannot defeat timeout; tags cannot wrap/reuse. */
 ready(false);clock_us=UINT32_MAX-5000;assert(pmw_write(7,4,payload,512)==PMW_OK);checks++;
 ready(false);clock_us=UINT32_MAX-5000;fault_op=0x2a;fault_stage=MSC_STAGE_CMD;inject=TIMEOUT;
 assert(pmw_write(7,4,payload,512)==PMW_IO);assert(clock_us<3000000);blocked();checks++;
 ready(false);w.tag=UINT32_MAX;before=commands;assert(pmw_write(7,4,payload,512)==PMW_IO);assert(commands==before);blocked();checks++;
 ready(false);pmw_detach(2);assert(w.present);pmw_detach(1);assert(pmw_write(7,0,payload,512)==PMW_STALE);
 assert(pmw_attach(1,7)==PMW_STALE);assert(pmw_attach(CFG_TUH_DEVICE_MAX+1,8)==PMW_DENIED);
 assert(pmw_attach(1,8)==PMW_OK);assert(pmw_prepare(7)==PMW_STALE);assert(pmw_prepare(8)==PMW_OK);checks++;
 /* Forged/stale callbacks cannot complete the current command. */
 ready(false);w.pending=true;w.active_generation=w.generation;w.outcome=TRANSPORT_BAD;
 tuh_msc_complete_data_t c={0};c.user_arg=w.tag+1;
 assert(!pmw_complete(1,&c) && w.pending);assert(!pmw_complete(1,NULL) && w.pending);
 c.user_arg=w.tag;assert(!pmw_complete(2,&c) && w.pending);
 w.active_generation--;assert(!pmw_complete(1,&c) && w.pending);w.pending=false;checks++;
 /* Explicit flush failure is sticky even when the preceding write was clean. */
 ready(true);assert(pmw_write(7,8,payload,512)==PMW_OK);
 fault_op=0x35;fault_stage=MSC_STAGE_STATUS;inject=STATUS_FAIL;
 assert(pmw_flush(7)==PMW_IO);assert(!w.dirty);blocked();checks++;
 printf("PASS %u writer scenarios; actual TinyUSB MSC, mocked media; no hardware I/O\n",checks);
 return 0;
}
