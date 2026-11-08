/* Actual writer and MSC; USB/control callbacks are modelled, no hardware. */
#define main unused_writer_main
#include "recovery-base.c"
#undef main
static unsigned scenario,clears,toggles,attempts,cases;
static bool ctrl_pending,retried,suppress_status;
static tuh_xfer_t ctrl;
static tusb_control_request_t setup_copy;
enum {GOOD, CLEAR_REJECT, CLEAR_FAIL, CLEAR_STALL, CLEAR_LEN, CLEAR_TIMEOUT,
 CLEAR_TAG,CLEAR_ADDR,CLEAR_EP,CLEAR_SETUP,CLEAR_LATE,CLEAR_DETACH,TOGGLE_FAIL,
 RETRY_REJECT,RETRY_STALL,RETRY_TIMEOUT,RETRY_TAG,RETRY_SIG,RETRY_SHORT,
 RETRY_PHASE,RETRY_FAILED,VIA_HUB,OTHER_PORT,BUS_FAIL,HEADER,DUAL,NSCEN};
bool tuh_bus_info_get(uint8_t addr,tuh_bus_info_t *out) {
 assert(addr==1);memset(out,0,sizeof(*out));out->hub_addr=scenario==VIA_HUB?2:0;
 out->rhport=scenario==OTHER_PORT?1:0;return scenario!=BUS_FAIL;
}
uint8_t usbh_get_rhport(uint8_t addr) {assert(addr==1);return 0;}
bool hcd_edpt_clear_stall(uint8_t port,uint8_t addr,uint8_t ep) {
 assert(!port && addr==1 && ep==0x81 && !ctrl_pending && !queued.set);
 assert(w.pending && w.present && !w.fault);toggles++;
 return scenario!=TOGGLE_FAIL;
}
bool tuh_control_xfer(tuh_xfer_t *x) {
 static const uint8_t expected[8]={2,1,0,0,0x81,0,0,0};
 assert(x && x->daddr==1 && !x->ep_addr && !x->buffer && x->complete_cb);
 assert(!memcmp(x->setup,expected,8) && x->user_data==w.tag);
 assert(!ctrl_pending && !queued.set);clears++;
 if(scenario==CLEAR_REJECT)return false;
 ctrl=*x;setup_copy=*x->setup;ctrl.setup=&setup_copy;ctrl_pending=true;return true;
}
bool usbh_edpt_xfer_with_callback(uint8_t d,uint8_t ep,uint8_t *b,uint16_t n,tuh_xfer_cb_t cb,uintptr_t u) {
 if(get_itf(1)->stage==MSC_STAGE_STATUS && w.recovery_tried) {
  assert(ep==0x81 && n==13);retried=true;
  if(scenario==RETRY_REJECT)return false;
 }
 return base_xfer(d,ep,b,n,cb,u);
}
static void header(unsigned op,unsigned stage,uint8_t *b,unsigned *n) {
 if(stage==MSC_STAGE_DATA && (op==0x5a || op==0x1a)) {
  memset(b,0,*n);*n=op==0x5a?8:4;b[op==0x5a?1:0]=op==0x5a?6:3;
 }
}
void pmw_pump(void) {
 if(ctrl_pending) {
  clock_us+=10000;
  if(scenario==CLEAR_TIMEOUT)return;
  tuh_xfer_t x=ctrl;x.result=XFER_RESULT_SUCCESS;x.actual_len=0;
  if(scenario==CLEAR_FAIL)x.result=XFER_RESULT_FAILED;
  if(scenario==CLEAR_STALL)x.result=XFER_RESULT_STALLED;
  if(scenario==CLEAR_LEN)x.actual_len=8; /* SETUP size is not DATA actual_len */
  if(scenario==CLEAR_TAG)x.user_data++;
  if(scenario==CLEAR_ADDR)x.daddr=2;
  if(scenario==CLEAR_EP)x.ep_addr=0x81;
  tusb_control_request_t setup=*x.setup;
  if(scenario==CLEAR_SETUP){setup.wIndex^=1;x.setup=&setup;}
  if(scenario==CLEAR_LATE)clock_us=w.started+2000000;
  if(scenario==CLEAR_DETACH)pmw_detach(1);
  ctrl_pending=false;x.complete_cb(&x);return;
 }
 msch_interface_t *m=get_itf(1);
 unsigned op=get_epbuf(1)->cbw.command[0];
 if(!suppress_status && queued.set && m->stage==MSC_STAGE_STATUS && (op==0x5a || (op==0x1a && scenario==DUAL))) {
  fault_op=op;fault_stage=MSC_STAGE_STATUS;
  if(!w.recovery_tried) {
   /* Complete an actual zero-byte STALL. No command resubmission. */
   attempts++;queued.set=false;clock_us+=10000;
   (void)msch_xfer_cb(1,0x81,XFER_RESULT_STALLED,0);return;
  }
  inject=scenario==RETRY_STALL?STALL:scenario==RETRY_TIMEOUT?TIMEOUT:
   scenario==RETRY_TAG?TAG:scenario==RETRY_SIG?SIGNATURE:
   scenario==RETRY_SHORT?SHORT:scenario==RETRY_PHASE?STATUS_PHASE:
   scenario==RETRY_FAILED?STATUS_FAIL:NONE;
 }
 base_pump();
}
static void init(unsigned s) {
 fresh(false);scenario=s;clears=toggles=attempts=0;retried=ctrl_pending=suppress_status=false;
 if(s==HEADER || s==DUAL)reply_hook=header;
}
int main(void) {
 for(unsigned s=0;s<NSCEN;s++) {
  init(s);int r=pmw_prepare(7);bool success=s==GOOD||s==HEADER||s==DUAL;
  if(success) {
   assert(r==PMW_OK && w.prepared && !w.fault);
   assert(clears==(s==DUAL?2u:1u) && toggles==clears && attempts==clears);
   unsigned n=commands;assert(pmw_write(7,0,payload,512)==PMW_DENIED);
   assert(commands==n && !writes);assert(pmw_release(7)==PMW_OK);
  } else {
   if(r!=(s==RETRY_FAILED?PMW_DENIED:PMW_IO))fprintf(stderr,"scenario %u result %d\n",s,r);
   assert(r==(s==RETRY_FAILED?PMW_DENIED:PMW_IO));assert(!w.prepared && !writes);
   assert(clears<2);
   if(w.fault) {
    /* A delayed successful clear must not restart CSW after fault/deadline. */
    unsigned n=commands,t=toggles;ctrl_pending=false;ctrl.result=XFER_RESULT_SUCCESS;ctrl.actual_len=0;
    if(clears && s!=CLEAR_REJECT)clear_complete(&ctrl);
    assert(commands==n && toggles==t);blocked();
   }
  }
  assert(!writes && !w.dirty);cases++;
 }
 /* Nonqualifying events never invoke clear/retry, including nonzero STALL bytes. */
 for(unsigned stage=MSC_STAGE_CMD;stage<=MSC_STAGE_STATUS;stage++)
 for(unsigned how=FAILED;how<=STALL;how++) {
  init(GOOD);suppress_status=true;fault_op=0x5a;fault_stage=stage;inject=how;
  assert(pmw_prepare(7)==PMW_IO && !clears && !toggles && !writes);cases++;
 }
 for(unsigned opi=0;opi<2;opi++) {
  init(GOOD);suppress_status=true;cache_on=true;
  fault_op=opi?0x35:0x25;fault_stage=MSC_STAGE_STATUS;inject=STALL;
  assert(pmw_prepare(7)==PMW_IO && !clears && !writes);cases++;
 }
 init(GOOD);clock_us=UINT32_MAX-5000;
 assert(pmw_prepare(7)==PMW_OK && clears==1 && !writes);cases++;
 init(GOOD);cache_on=true;
 assert(pmw_prepare(7)==PMW_OK && syncs==1 && clears==1 && !writes);cases++;
 /* Callback/command guards with actual MSC state and captured command. */
 init(GOOD);assert(pmw_prepare(7)==PMW_OK);
 for(unsigned k=0;k<16;k++) {
  msch_interface_t *m=get_itf(1);msch_interface_t saved=*m;
  msc_cbw_t *b=&get_epbuf(1)->cbw,copy=*b;
  m->stage=MSC_STAGE_IDLE;m->trace_stage_event=0x32;m->trace_ep=0x81;m->trace_count=0;
  switch(k) {
   case 0:m->configured=false;break;case 1:m->mounted=false;break;
   case 2:m->stage=MSC_STAGE_STATUS;break;case 3:m->buffer=NULL;break;
   case 4:m->complete_cb=NULL;break;case 5:b->tag++;break;
   case 6:b->command[0]=0x2a;break;case 7:m->data_result=XFER_RESULT_STALLED;break;
   case 8:m->data_xferred=65;break;case 9:m->trace_stage_event=0x22;break;
   case 10:m->trace_ep=0x82;break;case 11:m->trace_count=1;break;
   case 12:b->dir=0;break;case 13:b->total_bytes=512;break;
   case 14:b->lun=1;break;case 15:b->signature^=1;break;
  }
  assert(!tuh_msc_raw_stalled_csw_ep(1,&w.command));
  assert(!tuh_msc_raw_retry_csw(1,&w.command));*m=saved;*b=copy;cases++;
 }
 assert(!tuh_msc_raw_stalled_csw_ep(0,&w.command));
 assert(!tuh_msc_raw_stalled_csw_ep(CFG_TUH_DEVICE_MAX+1,&w.command));
 assert(!tuh_msc_raw_stalled_csw_ep(1,NULL));
 printf("PASS %u bounded CSW recovery cases; no sector writes\n",cases);
}
