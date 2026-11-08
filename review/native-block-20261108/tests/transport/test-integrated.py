from pathlib import Path
import subprocess,os
p=Path(__file__).resolve().parent;src=Path(os.environ['REVIEW_TINYUSB'])/'src'
s=Path(os.environ.get('PM_CONTROL_SOURCE',str(src/'host/usbh.c'))).read_text();a=s.index('static void _control_xfer_complete(');b=s.index('bool tuh_edpt_xfer(tuh_xfer_t* xfer)',a);actual=s[a:b]
# Keep the existing actual writer/MSC tests, and drive successful clears through
# the real host-control completion functions instead of constructing their result.
test=(p/'test-recovery.c').read_text()
engine=r'''
#undef TU_LOG_USBH
#define TU_LOG_USBH(...) ((void)0)
#undef TU_LOG_BUF_USBH
#define TU_LOG_BUF_USBH(...) ((void)0)
#undef TU_LOG_MEM_USBH
#define TU_LOG_MEM_USBH(...) ((void)0)
#undef osal_mutex_lock
#undef osal_mutex_unlock
#define osal_mutex_lock(...) ((void)0)
#define osal_mutex_unlock(...) ((void)0)
#define USBH_CONTROL_RETRY_MAX 3
#define CONTROL_STAGE_IDLE 0
#define CONTROL_STAGE_SETUP 1
#define CONTROL_STAGE_DATA 2
#define CONTROL_STAGE_ACK 3
typedef struct {unsigned stage,daddr,actual_len,failed_count;uint8_t *buffer;
 tuh_xfer_cb_t complete_cb;uintptr_t user_data;} usbh_ctrl_xfer_info_t;
static struct {usbh_ctrl_xfer_info_t ctrl_xfer_info;} _usbh_data;
static struct {tusb_control_request_t request;} _usbh_epbuf;
static unsigned control_ack_requests;
static void _control_set_xfer_stage(unsigned stage){_usbh_data.ctrl_xfer_info.stage=stage;}
bool tuh_connected(uint8_t d){return d==1;}
bool hcd_edpt_xfer(uint8_t port,uint8_t d,uint8_t ep,uint8_t *b,uint16_t len){
 assert(!port && d==1 && ep==0x80 && !b && !len);control_ack_requests++;return true;
}
static bool usbh_setup_send(uint8_t d,const uint8_t *b){(void)d;(void)b;assert(0);return false;}
bool tuh_edpt_abort_xfer(uint8_t d,uint8_t ep){(void)d;(void)ep;assert(0);return false;}
'''
test=test.replace('static unsigned scenario,clears,toggles,attempts,cases;',engine+actual+'\nstatic unsigned scenario,clears,toggles,attempts,cases;')
needle='ctrl_pending=false;x.complete_cb(&x);return;'
replace='''ctrl_pending=false;
  if(scenario==GOOD || scenario==HEADER || scenario==DUAL) {
   _usbh_epbuf.request=*x.setup;
   _usbh_data.ctrl_xfer_info=(usbh_ctrl_xfer_info_t){.stage=CONTROL_STAGE_SETUP,
    .daddr=1,.complete_cb=x.complete_cb,.user_data=x.user_data};
   unsigned n=control_ack_requests;
   assert(usbh_control_xfer_cb(1,0,XFER_RESULT_SUCCESS,8));
   assert(_usbh_data.ctrl_xfer_info.stage==CONTROL_STAGE_ACK && control_ack_requests==n+1);
   assert(usbh_control_xfer_cb(1,0x80,XFER_RESULT_SUCCESS,0));
  } else x.complete_cb(&x);
  return;'''
assert needle in test;test=test.replace(needle,replace).replace('bounded CSW recovery cases','integrated host-control/MSC/writer recovery cases')
(p/'test-integrated.c').write_text(test)
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-pthread','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections',('-Wl,-dead_strip' if __import__('sys').platform == 'darwin' else '-Wl,--gc-sections'),'-DPM_RAW_USB_CSW_RECOVERY=1','-DPM_RAW_USB_RP2_COMPARE=1','-DPM_RAW_USB_EMPTY_CACHE_WT=1','-DPMW_PINNED_MSC','-DPM_RAW_USB_RO','-DPM_RAW_USB_RW_LAB','-DPMW_MSC_SOURCE="'+str(src.resolve()/'class/msc/msc_host.c')+'"','-I'+str(p),'-I'+str(src),'-I'+str(Path(os.environ['REVIEW_FIRMWARE'])/'tests/rawblock/stubs'),str(p/'test-integrated.c'),'-o',str(p/'test-integrated')]
subprocess.run(cmd,check=True);subprocess.run([str(p/'test-integrated')],check=True,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
