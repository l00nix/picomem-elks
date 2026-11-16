/* Actual MSC production handler, synthetic HCD. No hardware access. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "class/msc/msc_host.c"

static unsigned callbacks, submits, fail_submit, configured, tests, failures;
static uint8_t payload[512], enum_buffer[512];
static tuh_msc_complete_data_t observed;
static msc_csw_t observed_csw;
static bool chain, chain_ok;
static uint8_t *submitted_buffer;
static uint16_t submitted_length;

bool usbh_edpt_xfer_with_callback(uint8_t d, uint8_t ep, uint8_t *buf,
    uint16_t len, tuh_xfer_cb_t cb, uintptr_t arg) {
  (void)ep; (void)cb; (void)arg; assert(d==1);
  submitted_buffer=buf; submitted_length=len;
  return ++submits != fail_submit;
}
bool usbh_edpt_claim(uint8_t d,uint8_t ep) {assert(d==1&&ep==2);return true;}
bool usbh_edpt_release(uint8_t d,uint8_t ep) {assert(d==1&&ep==2);return true;}
bool usbh_edpt_busy(uint8_t d,uint8_t ep) {(void)d;(void)ep;return false;}
uint8_t *usbh_get_enum_buf(void) {return enum_buffer;}
void usbh_driver_set_config_complete(uint8_t d,uint8_t itf) {(void)itf;assert(d==1);configured++;}
static bool done(uint8_t d,const tuh_msc_complete_data_t *p) {
  assert(d==1); callbacks++; observed=*p; observed_csw=*p->csw;
  if(chain) {chain=false;chain_ok=tuh_msc_test_unit_ready(1,0,done,0);}
  return true;
}
static void check(const char *name,bool ok) {
  tests++; if(!ok) failures++;
  printf("%s{\"case\":\"%s\",\"pass\":%s}",tests>1?",\n":"",name,ok?"true":"false");
}
static void reset(void) {
  msch_init(); msch_interface_t *p=get_itf(1);
  p->configured=p->mounted=true;p->ep_in=0x81;p->ep_out=2;
  p->capacity[0].block_size=512;p->capacity[0].block_count=128;
  callbacks=submits=fail_submit=configured=0;chain=chain_ok=false;
  memset(&observed,0,sizeof(observed));memset(payload,0xa5,sizeof(payload));
}
static bool block(bool read) {
  return read?tuh_msc_read10(1,0,payload,0,1,done,0):tuh_msc_write10(1,0,payload,0,1,done,0);
}
static void cbw(void) {assert(msch_xfer_cb(1,2,XFER_RESULT_SUCCESS,31));}
static void data(bool read,xfer_result_t result,unsigned count) {
  assert(msch_xfer_cb(1,read?0x81:2,result,count));
}
static void csw(xfer_result_t result,unsigned count,unsigned status,unsigned residue,int bad) {
  msc_csw_t v={.signature=MSC_CSW_SIGNATURE,.tag=get_epbuf(1)->cbw.tag,
    .status=status,.data_residue=residue};
  if(bad==1)v.signature++; if(bad==2)v.tag++;
  assert(count<=sizeof(v));memcpy(submitted_buffer,&v,count);
  assert(msch_xfer_cb(1,0x81,result,count));
}
static bool exact_ok(void) {
  return callbacks==1&&observed.result==XFER_RESULT_SUCCESS&&observed.actual_len==512&&
    observed_csw.status==0&&observed_csw.data_residue==0;
}
static bool fenced(void) {
  unsigned before=callbacks;
  bool blocked=!block(true)&&!tuh_msc_ready(1);
  bool late=!msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,13);
  return blocked&&late&&callbacks==before&&before==1;
}
int main(void) {
  puts("[");
  for(unsigned read=0;read<2;read++) {
    char name[100];
    reset();assert(block(read));cbw();data(read,XFER_RESULT_SUCCESS,512);csw(XFER_RESULT_SUCCESS,13,0,0,0);
    snprintf(name,sizeof(name),"healthy_%s",read?"read":"write");check(name,exact_ok());
    reset();assert(block(read));cbw();data(read,XFER_RESULT_SUCCESS,256);csw(XFER_RESULT_SUCCESS,13,0,0,0);
    snprintf(name,sizeof(name),"short_data_%s_rejected_by_exact_consumer",read?"read":"write");
    check(name,!exact_ok()&&callbacks==1&&observed.result==XFER_RESULT_SUCCESS&&observed.actual_len==256);
    const xfer_result_t results[]={XFER_RESULT_FAILED,XFER_RESULT_STALLED};
    for(unsigned r=0;r<2;r++) {
      reset();assert(block(read));cbw();data(read,results[r],0);
      snprintf(name,sizeof(name),"data_%u_%s_fenced",r,read?"read":"write");
      check(name,observed.result!=XFER_RESULT_SUCCESS&&fenced());
    }
  }
  const xfer_result_t statuses[]={XFER_RESULT_FAILED,XFER_RESULT_STALLED,XFER_RESULT_SUCCESS};
  const unsigned lengths[]={0,0,12};
  for(unsigned i=0;i<3;i++) {
    reset();assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,512);csw(statuses[i],lengths[i],0,0,0);
    const char *names[]={"failed_csw","stalled_csw","short_csw"};
    check(names[i],observed.result!=XFER_RESULT_SUCCESS&&fenced());
  }
  for(unsigned i=0;i<2;i++) {
    reset();assert(block(true));assert(msch_xfer_cb(1,2,i?XFER_RESULT_SUCCESS:XFER_RESULT_FAILED,i?30:0));
    check(i?"short_cbw":"failed_cbw",observed.result!=XFER_RESULT_SUCCESS&&fenced());
  }
  for(unsigned bad=1;bad<3;bad++) {
    reset();assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,512);csw(XFER_RESULT_SUCCESS,13,0,0,bad);
    check(bad==1?"csw_bad_signature":"csw_bad_tag",observed.result!=XFER_RESULT_SUCCESS&&fenced());
  }
  reset();assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,512);csw(XFER_RESULT_SUCCESS,13,0,513,0);
  check("csw_residue_exceeds_request",observed.result!=XFER_RESULT_SUCCESS&&fenced());
  reset();assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,512);csw(XFER_RESULT_SUCCESS,13,0,1,0);
  check("nonzero_residue_rejected_by_exact_consumer",!exact_ok()&&callbacks==1);
  reset();assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,512);csw(XFER_RESULT_SUCCESS,13,1,0,0);
  check("scsi_failure_distinct_from_transport",observed.result==XFER_RESULT_SUCCESS&&observed_csw.status==1&&!exact_ok()&&tuh_msc_ready(1));
  reset();assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,512);csw(XFER_RESULT_SUCCESS,13,2,0,0);
  check("phase_error_fences_next_command",fenced());
  for(unsigned fail=1;fail<=3;fail++) {
    reset();fail_submit=fail;bool ok=block(true);
    if(fail==1)check("cbw_submission_false_no_callback",!ok&&callbacks==0&&tuh_msc_ready(1));
    else {assert(ok);cbw();if(fail==3)data(true,XFER_RESULT_SUCCESS,512);
      check(fail==2?"data_submission_false":"csw_submission_false",observed.result!=XFER_RESULT_SUCCESS&&fenced());}
  }
  reset();assert(tuh_msc_inquiry(1,0,(scsi_inquiry_resp_t*)payload,done,0));cbw();data(true,XFER_RESULT_SUCCESS,18);csw(XFER_RESULT_SUCCESS,13,0,18,0);
  check("legitimate_short_inquiry",callbacks==1&&observed.result==XFER_RESULT_SUCCESS&&observed.actual_len==18&&tuh_msc_ready(1));
  assert(tuh_msc_test_unit_ready(1,0,done,0));cbw();csw(XFER_RESULT_SUCCESS,13,0,0,0);
  check("no_data_resets_actual_count",callbacks==2&&observed.result==XFER_RESULT_SUCCESS&&observed.actual_len==0);
  reset();assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,512);chain=true;csw(XFER_RESULT_SUCCESS,13,0,0,0);
  check("completion_can_submit_next_command",callbacks==1&&chain_ok&&get_itf(1)->stage==MSC_STAGE_CMD);
  cbw();csw(XFER_RESULT_SUCCESS,13,0,0,0);
  check("chained_command_completes_once",callbacks==2&&observed.actual_len==0);
  for(unsigned mode=0;mode<4;mode++) {
    reset();get_itf(1)->mounted=false;
    scsi_read_capacity10_resp_t value={.last_lba=tu_htonl(127),.block_size=tu_htonl(512)};
    assert(tuh_msc_read_capacity(1,0,(scsi_read_capacity10_resp_t*)enum_buffer,config_read_capacity_complete,0));cbw();
    memcpy(enum_buffer,&value,sizeof(value));
    data(true,mode==2?XFER_RESULT_FAILED:XFER_RESULT_SUCCESS,mode==1?4:8);
    if(mode!=2)csw(XFER_RESULT_SUCCESS,13,0,mode==3?1:0,0);
    const char *names[]={"capacity_healthy","capacity_short_rejected","capacity_transport_failure_rejected","capacity_residue_rejected"};
    check(names[mode],mode==0?configured==1&&tuh_msc_mounted(1)&&tuh_msc_get_block_count(1,0)==128:
      configured==0&&!tuh_msc_mounted(1));
  }
  for(unsigned stage=0;stage<3;stage++) {
    reset();assert(block(true));if(stage>0)cbw();if(stage>1)data(true,XFER_RESULT_SUCCESS,512);
    unsigned n=stage==0?31:stage==1?512:13;
    assert(msch_xfer_cb(1,stage==0?0x81:2,XFER_RESULT_SUCCESS,n));
    const char *names[]={"cbw_wrong_endpoint","data_wrong_endpoint","csw_wrong_endpoint"};
    check(names[stage],observed.result!=XFER_RESULT_SUCCESS&&fenced());
  }
  reset();assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,513);
  check("oversized_data_rejected",observed.result!=XFER_RESULT_SUCCESS&&fenced());
  reset();assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,512);csw(XFER_RESULT_SUCCESS,13,0,0,0);
  callbacks=0;assert(block(true));cbw();data(true,XFER_RESULT_SUCCESS,512);csw(XFER_RESULT_FAILED,0,0,0,0);
  check("previous_success_cannot_supply_stale_csw",observed.result!=XFER_RESULT_SUCCESS&&fenced());
  reset();assert(block(true));cbw();data(true,XFER_RESULT_FAILED,0);
  unsigned before_callbacks=callbacks;
  msc_csw_t late={.signature=MSC_CSW_SIGNATURE,.tag=get_epbuf(1)->cbw.tag,.status=0};
  get_epbuf(1)->csw=late;
  check("good_late_csw_cannot_override_data_failure",!msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,13)&&callbacks==before_callbacks&&observed.result!=XFER_RESULT_SUCCESS&&fenced());
  reset();get_itf(1)->mounted=false;
  assert(tuh_msc_test_unit_ready(1,0,config_test_unit_ready_complete,0));
  assert(msch_xfer_cb(1,2,XFER_RESULT_FAILED,0));
  check("enum_tur_transport_failure_does_not_retry",submits==1&&!get_itf(1)->mounted&&get_itf(1)->stage==MSC_STAGE_FAILED);
  reset();get_itf(1)->mounted=false;
  assert(tuh_msc_request_sense(1,0,enum_buffer,config_request_sense_complete,0));cbw();data(true,XFER_RESULT_FAILED,0);
  check("enum_sense_transport_failure_does_not_retry",submits==2&&!get_itf(1)->mounted&&get_itf(1)->stage==MSC_STAGE_FAILED);
  printf("\n]\n");fprintf(stderr,"%u tests, %u failures\n",tests,failures);
  return failures?1:0;
}
