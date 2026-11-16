// Independent boundary harness. The included implementation is extracted verbatim.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <algorithm>
#include <vector>
#include <string>
using BYTE=uint8_t;using LBA_t=uint32_t;using UINT=unsigned;using DSTATUS=unsigned;using DRESULT=unsigned;
enum {STA_NODISK=2,RES_NOTRDY=3,XFER_RESULT_SUCCESS=0,XFER_RESULT_FAILED=1,MSC_CSW_STATUS_PASSED=0};
#define MSC_CSW_SIGNATURE 0x53425355u
#define MSC_CBW_SIGNATURE 0x43425355u
struct msc_cbw_t {uint32_t signature=0,tag=0,total_bytes=0;uint8_t dir=0,lun=0,cmd_len=0,command[16]={};};
struct csw_t {uint32_t signature,tag;uint8_t status;uint32_t data_residue;};
struct tuh_msc_complete_data_t {int result;unsigned actual_len;csw_t *csw;msc_cbw_t *cbw;uintptr_t user_arg;};
using Callback=bool(*)(uint8_t,const tuh_msc_complete_data_t*);
enum Kind {READ,WRITE,FLUSH,QUERY};
struct Spec {bool submit=true,never=false,disconnect=false;int result=0,len=-1;unsigned residue=0,status=0,signature=MSC_CSW_SIGNATURE,tag=7,addr=1;uint32_t delay=0;};
struct Job {Kind kind;Spec spec;Callback cb;uintptr_t arg;uint8_t *buffer;bool delivered=false;uint32_t submitted_at;};
static std::vector<Spec> specs;static std::vector<Job> jobs;
static std::vector<unsigned> pio_sizes;static std::vector<uint8_t> captured;
static unsigned tests,failures,pio_calls,fail_pio,ready_calls,expire_ready_on,write_calls,flush_calls,read_calls;
static bool ready_flag,mounted_flag;static uint32_t block_size,block_count,clock_us,lba_input;
static bool stale_data_bad;
void tuh_msc_umount_cb(uint8_t);
static uint8_t mode_reply[24];static uint32_t pio_delay;static bool disconnect_during_pio;static std::vector<char> events;
static uint8_t pattern(unsigned i){return (i*37+11)&255;}
uint32_t tuh_msc_get_block_size(uint8_t,unsigned){return block_size;}
uint32_t tuh_msc_get_block_count(uint8_t,unsigned){return block_count;}
static std::vector<std::string> statuses;
void usb_set_status(uint8_t,const char *fmt,...){char b[128];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);statuses.emplace_back(b);}
bool tuh_msc_ready(uint8_t){if(++ready_calls==expire_ready_on)clock_us+=2000000u;return ready_flag;}
bool tuh_msc_mounted(uint8_t){return mounted_flag;}
uint32_t time_us_32(){return ++clock_us;}
bool dev_pmio_do_rw(uint8_t *buf,uint16_t size,bool read,bool wide){
 assert(!wide);++pio_calls;pio_sizes.push_back(size);events.push_back(read?'O':'P');clock_us+=pio_delay;if(disconnect_during_pio)tuh_msc_umount_cb(1);
 if(pio_calls==fail_pio){if(!read&&size)buf[0]=0xcc;return false;}
 if(read){assert(size==8||size==512);captured.assign(buf,buf+size);}
 else {assert(size==4||size==516);for(unsigned i=0;i<4;i++)buf[i]=lba_input>>(8*i);if(size==516)for(unsigned i=0;i<512;i++)buf[4+i]=pattern(i);}
 return true;
}
static bool submit(Kind kind,uint8_t *buf,Callback cb,uintptr_t arg){
 Spec s=jobs.size()<specs.size()?specs[jobs.size()]:Spec{};
 jobs.push_back({kind,s,cb,arg,buf,false,clock_us});
 if(kind==WRITE)++write_calls;else if(kind==FLUSH)++flush_calls;else if(kind==READ)++read_calls;
 events.push_back(kind==QUERY?'Q':kind==FLUSH?'F':kind==WRITE?'W':'R');
 return s.submit;
}
bool tuh_msc_read10(uint8_t addr,unsigned lun,uint8_t *buf,uint32_t lba,unsigned n,Callback cb,uintptr_t arg){
 assert(addr==1&&lun==0&&lba==lba_input&&n==1&&arg==512);assert(uintptr_t(buf)%4==0);return submit(READ,buf,cb,arg);
}
bool tuh_msc_write10(uint8_t addr,unsigned lun,const uint8_t *buf,uint32_t lba,unsigned n,Callback cb,uintptr_t arg){
 assert(addr==1&&lun==0&&lba==lba_input&&n==1&&arg==512);assert(uintptr_t(buf)%4==0);
 for(unsigned i=0;i<512;i++)if(buf[i]!=pattern(i))stale_data_bad=true;
 return submit(WRITE,const_cast<uint8_t*>(buf),cb,arg);
}
bool tuh_msc_scsi_command(uint8_t addr,const msc_cbw_t *cbw,void *buf,Callback cb,uintptr_t arg){
 assert(addr==1&&cbw->signature==MSC_CBW_SIGNATURE&&cbw->tag==0x54555342&&cbw->lun==0);
 if(cbw->command[0]==0x1a){
  assert(buf&&uintptr_t(buf)%4==0&&arg==24&&cbw->total_bytes==24&&cbw->dir==0x80&&cbw->cmd_len==6);
  const uint8_t expected[16]={0x1a,8,8,0,24,0};assert(memcmp(cbw->command,expected,16)==0);
  return submit(QUERY,static_cast<uint8_t*>(buf),cb,arg);
 }
 assert(buf==nullptr&&arg==0&&cbw->total_bytes==0&&cbw->dir==0&&cbw->cmd_len==10&&cbw->command[0]==0x35);
 for(unsigned i=1;i<16;i++)assert(cbw->command[i]==0);
 return submit(FLUSH,nullptr,cb,arg);
}
void tuh_task();
#include "production-body.inc"
static void deliver(size_t index,bool force_healthy=false){
 Job &j=jobs.at(index);Spec s=force_healthy?Spec{}:j.spec;
 if(j.kind==READ)for(unsigned i=0;i<512;i++)j.buffer[i]=pattern(i);
 if(j.kind==QUERY){unsigned actual=s.len<0?24:unsigned(s.len);memcpy(j.buffer,mode_reply,std::min(actual,24u));}
 msc_cbw_t cmd;cmd.tag=7;
 csw_t resp={s.signature,s.tag,(uint8_t)s.status,s.residue};
 tuh_msc_complete_data_t reply={s.result,s.len<0?unsigned(j.arg):unsigned(s.len),&resp,&cmd,j.arg};
 j.delivered=true;j.cb(s.addr,&reply);
}
void tuh_task(){
 assert(!jobs.empty());Job &j=jobs.back();assert(j.spec.submit&&!j.delivered);
 if(j.spec.disconnect){tuh_msc_umount_cb(1);return;}
 if(j.spec.never){clock_us+=500000;return;}
 clock_us+=j.spec.delay;deliver(jobs.size()-1);
}
static void reset(){
 disk=0;mode_header_only=false;statuses.clear();selected=bound=fault=busy=good=write_ready=write_cache_enabled=false;sectors=0;memset(data,0,sizeof(data));
 ready_flag=mounted_flag=true;block_size=512;block_count=128;clock_us=0;lba_input=3;
 pio_calls=fail_pio=ready_calls=expire_ready_on=write_calls=flush_calls=read_calls=0;stale_data_bad=false;
 jobs.clear();specs.clear();pio_sizes.clear();captured.clear();events.clear();pio_delay=0;disconnect_during_pio=false;
 memset(mode_reply,0,24);mode_reply[0]=23;mode_reply[4]=8;mode_reply[5]=18;
}
static void mounted(){reset();tuh_msc_mount_cb(1);bound=true;}
static bool probe(){return pm_usb_ssd_command(3)&&pm_usb_ssd_command(4);}
static void check(const std::string &name,bool ok){++tests;if(!ok)++failures;printf("%s{\"case\":\"%s\",\"pass\":%s}",tests>1?",\n":"",name.c_str(),ok?"true":"false");}
static bool fenced(){auto p=pio_calls;auto n=jobs.size();auto copy=std::vector<uint8_t>(data,data+sizeof(data));bool ok=!pm_usb_ssd_command(3)&&!pm_usb_ssd_command(4)&&!pm_usb_ssd_command(0)&&!pm_usb_ssd_command(1)&&!pm_usb_ssd_command(2);return ok&&p==pio_calls&&n==jobs.size()&&copy==std::vector<uint8_t>(data,data+sizeof(data));}
static bool order(std::initializer_list<Kind> expected){if(jobs.size()!=expected.size())return false;unsigned i=0;for(auto k:expected)if(jobs[i++].kind!=k)return false;return true;}
int main(){puts("[");
 reset();check("unmounted_no_exchange",!pm_usb_ssd_command(0)&&!pm_usb_ssd_command(2)&&pio_calls==0);
 mounted();check("geometry",probe()&&captured.size()==8&&captured[0]=='P'&&captured[1]=='M'&&captured[2]==128&&captured[6]==0&&captured[7]==2);
 mounted();bool ok=pm_usb_ssd_command(1);bool bytes=captured.size()==512;for(unsigned i=0;bytes&&i<512;i++)bytes=captured[i]==pattern(i);check("healthy_read_offset4",ok&&bytes&&read_calls==1&&jobs[0].buffer==data+4);
 mounted();lba_input=128;check("read_out_of_range",!pm_usb_ssd_command(1)&&jobs.empty()&&fenced());
 reset();block_size=4096;tuh_msc_mount_cb(1);check("unsupported_sector_size",!fault&&!sectors&&!bound);
 reset();block_count=0;tuh_msc_mount_cb(1);check("zero_capacity",!fault&&!sectors&&!bound);
 mounted();tuh_msc_mount_cb(2);check("second_device_ignored",disk==1&&sectors==128);
 mounted();tuh_msc_umount_cb(2);check("unrelated_disconnect",!fault&&probe());
 mounted();tuh_msc_umount_cb(1);tuh_msc_mount_cb(1);check("selected_disconnect",fault&&fenced());
 std::vector<std::pair<std::string,Spec>> bad;
 Spec s;s.submit=false;bad.push_back({"submission",s});s={};s.never=true;bad.push_back({"timeout",s});s={};s.disconnect=true;bad.push_back({"disconnect",s});s={};s.result=1;bad.push_back({"result_failed",s});s={};s.result=2;bad.push_back({"result_non_success",s});s={};s.len=0;bad.push_back({"length_zero",s});s={};s.len=511;bad.push_back({"length_short",s});s={};s.len=513;bad.push_back({"length_long",s});s={};s.residue=1;bad.push_back({"residue",s});s={};s.status=1;bad.push_back({"scsi_failure",s});s={};s.status=2;bad.push_back({"phase_error",s});s={};s.signature=0;bad.push_back({"signature",s});s={};s.tag=8;bad.push_back({"tag",s});s={};s.addr=2;bad.push_back({"wrong_device",s});
 for(const auto &b:bad){mounted();specs={b.second};check("read_"+b.first,!pm_usb_ssd_command(1)&&fault&&captured.empty()&&fenced());}
 for(unsigned p=1;p<=2;p++){mounted();fail_pio=p;check(p==1?"read_lba_PIO_fail":"read_output_PIO_fail",!pm_usb_ssd_command(1)&&fault&&fenced());}
 mounted();fail_pio=1;check("geometry_PIO_fail",!probe()&&fault&&fenced());
 mounted();check("unknown_operation",!pm_usb_ssd_command(99)&&pio_calls==0&&fenced());
 reset();check("fatfs_excluded",usb_disk_status()==STA_NODISK&&usb_disk_initialize()==STA_NODISK&&usb_disk_read(nullptr,0,1)==RES_NOTRDY&&usb_disk_write(nullptr,0,1)==RES_NOTRDY&&usb_disk_ioctl(0,nullptr)==RES_NOTRDY);

 mounted();ok=pm_usb_ssd_command(2);check("WCE0_query_input_write_payload",ok&&order({QUERY,WRITE})&&events==std::vector<char>({'Q','P','W'})&&pio_sizes==std::vector<unsigned>({516})&&jobs[0].buffer==data+4&&jobs[1].buffer==data+4&&!stale_data_bad&&write_ready&&!write_cache_enabled&&flush_calls==0);
 ok=pm_usb_ssd_command(2);check("WCE0_cached_policy",ok&&order({QUERY,WRITE,WRITE})&&write_calls==2&&!stale_data_bad&&flush_calls==0);
 mounted();mode_reply[6]=4;ok=pm_usb_ssd_command(2);check("WCE1_query_preflush_input_write_postflush",ok&&order({QUERY,FLUSH,WRITE,FLUSH})&&events==std::vector<char>({'Q','F','P','W','F'})&&write_ready&&write_cache_enabled&&!stale_data_bad);
 ok=pm_usb_ssd_command(2);check("WCE1_cached_policy_postflush",ok&&order({QUERY,FLUSH,WRITE,FLUSH,WRITE,FLUSH})&&write_calls==2&&flush_calls==3&&!stale_data_bad);
 mounted();mode_reply[4]=0x88;check("page_PS_bit_permitted",pm_usb_ssd_command(2)&&!write_cache_enabled);
 mounted();mode_reply[2]=0x10;check("non_WP_device_parameter_allowed",pm_usb_ssd_command(2));
 for(auto mutation:std::vector<std::pair<unsigned,unsigned>>{{0,0},{0,22},{0,24},{0,255},{1,1},{1,255},{2,128},{3,1},{3,8},{3,255},{4,0},{4,9},{4,0x48},{4,0xc8},{5,0},{5,17},{5,19},{5,255}}){
  mounted();mode_reply[mutation.first]=mutation.second;
  check("malformed_byte"+std::to_string(mutation.first)+"_"+std::to_string(mutation.second),!pm_usb_ssd_command(2)&&order({QUERY})&&pio_calls==0&&write_calls==0&&!write_ready&&fault&&fenced());
 }
 for(unsigned i=0;i<256;i++){
  mounted();mode_reply[6]=i;
  bool result=pm_usb_ssd_command(2);bool wce=(i&4)!=0;
  check("WCE_bit_decode_"+std::to_string(i),result&&write_cache_enabled==wce&&flush_calls==(wce?2u:0u)&&!stale_data_bad);
 }
 mounted();lba_input=127;check("last_sector_allowed",pm_usb_ssd_command(2)&&write_calls==1);
 for(uint32_t lba:std::vector<uint32_t>{128,0xffffffffu}){
  mounted();lba_input=lba;check("out_of_range_no_WRITE_"+std::to_string(lba),!pm_usb_ssd_command(2)&&order({QUERY})&&write_calls==0&&fault&&fenced());
 }
 mounted();mode_reply[6]=4;lba_input=128;check("WCE1_out_of_range_after_preparation",!pm_usb_ssd_command(2)&&order({QUERY,FLUSH})&&write_calls==0&&fenced());
 mounted();fail_pio=1;check("partial_input_after_query_no_WRITE",!pm_usb_ssd_command(2)&&order({QUERY})&&pio_sizes==std::vector<unsigned>({516})&&write_calls==0&&fault&&fenced());
 mounted();mode_reply[6]=4;fail_pio=1;check("partial_input_after_preflush_no_WRITE",!pm_usb_ssd_command(2)&&order({QUERY,FLUSH})&&write_calls==0&&fault&&fenced());
 for(unsigned stage=0;stage<4;stage++)for(const auto &b:bad){
  mounted();mode_reply[6]=4;Spec x=b.second;
  if(b.first=="length_zero")x.len=(stage==1||stage==3)?1:0;
  if(b.first=="length_short")x.len=stage==0?23:stage==2?511:1;
  if(b.first=="length_long")x.len=stage==0?25:stage==2?513:1;
  specs.resize(stage+1);specs[stage]=x;bool result=pm_usb_ssd_command(2);
  check("WCE1_stage"+std::to_string(stage)+"_"+b.first,!result&&fault&&jobs.size()==stage+1&&write_calls==(stage>=2?1u:0u)&&pio_calls==(stage>=2?1u:0u)&&write_ready==(stage>=2)&&fenced());
 }
 for(const auto &b:bad){mounted();specs.resize(2);specs[1]=b.second;check("WCE0_write_"+b.first,!pm_usb_ssd_command(2)&&order({QUERY,WRITE})&&fault&&flush_calls==0&&fenced());}
 for(unsigned stage=1;stage<=4;stage++){
  mounted();mode_reply[6]=4;expire_ready_on=stage;
  check("deadline_before_stage"+std::to_string(stage),!pm_usb_ssd_command(2)&&jobs.size()==stage-1&&write_calls==(stage==4?1u:0u)&&fault&&fenced());
 }
 mounted();specs.resize(1);specs[0].delay=2000000;check("query_expiry_no_input",!pm_usb_ssd_command(2)&&order({QUERY})&&pio_calls==0&&fault&&fenced());
 mounted();mode_reply[6]=4;specs.resize(2);specs[0].delay=1100000;specs[1].delay=900000;check("query_preflush_shared_deadline_no_input",!pm_usb_ssd_command(2)&&order({QUERY,FLUSH})&&pio_calls==0&&fault&&fenced());
 mounted();pio_delay=2000000;check("PIO_consumes_shared_deadline_no_WRITE",!pm_usb_ssd_command(2)&&order({QUERY})&&pio_calls==1&&write_calls==0&&fault&&fenced());
 mounted();mode_reply[6]=4;pio_delay=2000000;check("WCE1_PIO_expiry_no_WRITE",!pm_usb_ssd_command(2)&&order({QUERY,FLUSH})&&write_calls==0&&fault&&fenced());
 mounted();specs.resize(2);specs[0].delay=1200000;specs[1].delay=900000;check("query_WRITE_shared_deadline",!pm_usb_ssd_command(2)&&order({QUERY,WRITE})&&fault&&fenced());
 mounted();mode_reply[6]=4;specs.resize(4);specs[3].delay=2000000;check("late_postflush_not_success",!pm_usb_ssd_command(2)&&order({QUERY,FLUSH,WRITE,FLUSH})&&fault&&fenced());
 mounted();specs.resize(1);specs[0].never=true;ok=!pm_usb_ssd_command(2);deliver(0,true);check("late_query_DMA_no_input_or_reuse",ok&&fault&&!write_ready&&pio_calls==0&&write_calls==0&&fenced());
 mounted();specs.resize(2);specs[1].never=true;ok=!pm_usb_ssd_command(2);deliver(1,true);check("late_WRITE_callback_no_reuse",ok&&fault&&write_calls==1&&fenced());
 mounted();specs.resize(1);specs[0].never=true;ok=!pm_usb_ssd_command(1);deliver(0,true);check("late_READ_DMA_no_reuse",ok&&fault&&fenced());
 mounted();ok=pm_usb_ssd_command(2);tuh_msc_umount_cb(1);tuh_msc_mount_cb(1);check("policy_not_reused_after_disconnect",ok&&write_ready&&fault&&fenced());
 mounted();ready_flag=false;check("not_ready_no_query",!pm_usb_ssd_command(2)&&jobs.empty()&&pio_calls==0&&fault&&fenced());
 mounted();ok=pm_usb_ssd_command(2);specs.resize(3);specs[2].result=1;check("cached_WCE0_write_failure",ok&&!pm_usb_ssd_command(2)&&order({QUERY,WRITE,WRITE})&&fault&&fenced());
 mounted();mode_reply[6]=4;ok=pm_usb_ssd_command(2);specs.resize(5);specs[4].result=1;check("cached_WCE1_write_failure_no_flush",ok&&!pm_usb_ssd_command(2)&&order({QUERY,FLUSH,WRITE,FLUSH,WRITE})&&fault&&fenced());
 mounted();mode_reply[6]=4;ok=pm_usb_ssd_command(2);specs.resize(6);specs[5].status=1;check("cached_WCE1_postflush_failure",ok&&!pm_usb_ssd_command(2)&&order({QUERY,FLUSH,WRITE,FLUSH,WRITE,FLUSH})&&fault&&fenced());
 mounted();clock_us=0xffffff00u;check("deadline_wrap_success",pm_usb_ssd_command(2)&&order({QUERY,WRITE}));
 mounted();clock_us=0xffffff00u;specs.resize(1);specs[0].never=true;check("deadline_wrap_timeout",!pm_usb_ssd_command(2)&&order({QUERY})&&fault&&fenced());
 reset();block_count=0xffffffffu;tuh_msc_mount_cb(1);bound=true;lba_input=0x12345678u;check("full_32bit_lba_payload_survives_query",pm_usb_ssd_command(2)&&write_calls==1&&!stale_data_bad);
 mounted();ok=pm_usb_ssd_command(2);auto before=jobs.size();ok=ok&&probe()&&pm_usb_ssd_command(1);check("read_geometry_after_write",ok&&jobs.size()==before+1&&jobs.back().kind==READ&&captured.size()==512);
 ok=pm_usb_ssd_command(2);check("cached_policy_after_buffer_reuse",ok&&jobs.back().kind==WRITE&&!stale_data_bad&&flush_calls==0);
 mounted();disconnect_during_pio=true;check("disconnect_during_input_no_WRITE",!pm_usb_ssd_command(2)&&order({QUERY})&&fault&&write_calls==0&&fenced());
 mounted();mode_reply[6]=4;disconnect_during_pio=true;check("WCE1_disconnect_during_input_no_WRITE",!pm_usb_ssd_command(2)&&order({QUERY,FLUSH})&&fault&&write_calls==0&&fenced());
 mounted();ok=pm_usb_ssd_command(2);pio_delay=2000000;check("cached_policy_input_expiry_no_second_WRITE",ok&&!pm_usb_ssd_command(2)&&order({QUERY,WRITE})&&fault&&fenced());
 mounted();memset(data+4,0xff,24);mode_reply[4]=0;check("stale_nonzero_buffer_not_policy",!pm_usb_ssd_command(2)&&!write_ready&&write_calls==0&&pio_calls==0&&fenced());

 // Additional narrow header-only policy tests. Legacy cases above are retained.
 auto header=[](){mounted();memset(mode_reply,0,24);mode_reply[0]=3;specs.resize(1);specs[0].len=4;};
 auto assumed=[](){return std::count(statuses.begin(),statuses.end(),"USB SSD, assumed write-through");};
 for(unsigned residue:std::vector<unsigned>{0,20}){
  header();specs[0].residue=residue;bool r=pm_usb_ssd_command(2);
  check("header4_accept_residue"+std::to_string(residue),r&&order({QUERY,WRITE})&&events==std::vector<char>({'Q','P','W'})&&write_ready&&!write_cache_enabled&&flush_calls==0&&!stale_data_bad&&assumed()==1);
  r=pm_usb_ssd_command(2);check("header4_cached_residue"+std::to_string(residue),r&&order({QUERY,WRITE,WRITE})&&assumed()==1&&flush_calls==0&&!stale_data_bad);
 }
 for(unsigned len=0;len<=25;++len){
  header();specs[0].len=len;
  bool r=pm_usb_ssd_command(2);
  check("header_shape_length_"+std::to_string(len),len==4?(r&&write_calls==1):(!r&&pio_calls==0&&write_calls==0&&assumed()==0&&fault&&fenced()));
 }
 for(unsigned byte=0;byte<4;byte++)for(unsigned bit=0;bit<8;bit++){
  header();mode_reply[byte]^=1u<<bit;bool allowed=byte==2&&bit!=7;bool r=pm_usb_ssd_command(2);
  check("header_bit_"+std::to_string(byte)+"_"+std::to_string(bit),allowed?(r&&assumed()==1&&!write_cache_enabled):(!r&&pio_calls==0&&write_calls==0&&!write_ready&&assumed()==0&&fault&&fenced()));
 }
 for(unsigned residue=0;residue<=25;residue++){
  header();specs[0].residue=residue;bool r=pm_usb_ssd_command(2);bool allowed=residue==0||residue==20;
  check("header_residue_"+std::to_string(residue),allowed?(r&&assumed()==1):(!r&&pio_calls==0&&write_calls==0&&fault&&assumed()==0&&fenced()));
 }
 header();specs[0].residue=0xffffffffu;check("header_residue_uintmax",!pm_usb_ssd_command(2)&&fault&&fenced());
 for(const auto &b:bad){
  if(b.first=="length_zero"||b.first=="length_short"||b.first=="length_long")continue;
  header();Spec x=b.second;x.len=4;specs[0]=x;
  check("header_transport_"+b.first,!pm_usb_ssd_command(2)&&fault&&write_calls==0&&pio_calls==0&&!write_ready&&assumed()==0&&fenced());
 }
 // Query short-data writes only received bytes; stale trailing bytes cannot act as page.
 header();memset(data+4,0xff,24);check("header_ignores_unreceived_stale_tail",pm_usb_ssd_command(2)&&assumed()==1&&!stale_data_bad);
 header();mode_reply[6]=4;check("header_stale_tail_WCE_not_parsed",pm_usb_ssd_command(2)&&flush_calls==0&&!write_cache_enabled&&assumed()==1);
 mounted();specs.resize(1);specs[0].len=4;check("truncated_full_page_header_rejected",!pm_usb_ssd_command(2)&&fault&&write_calls==0&&pio_calls==0&&assumed()==0&&fenced());
 header();specs[0].len=0;memcpy(data+4,mode_reply,24);check("zero_actual_with_stale_valid_header_rejected",!pm_usb_ssd_command(2)&&fault&&assumed()==0&&fenced());
 mounted();mode_header_only=true;check("fresh_full_page_clears_old_mode_flag",pm_usb_ssd_command(2)&&!mode_header_only&&assumed()==0);
 header();mode_reply[0]=23;check("four_bytes_claim_full24_rejected",!pm_usb_ssd_command(2)&&fault&&fenced());
 mounted();specs.resize(1);specs[0].residue=20;check("full24_residue20_still_rejected",!pm_usb_ssd_command(2)&&fault&&fenced());
 mounted();check("full24_explicit_WCE0_no_assumption_log",pm_usb_ssd_command(2)&&!write_cache_enabled&&assumed()==0&&flush_calls==0);
 mounted();mode_reply[6]=4;check("full24_WCE1_no_assumption_log",pm_usb_ssd_command(2)&&write_cache_enabled&&assumed()==0&&flush_calls==2);
 header();specs.resize(2);specs[1].len=4;check("assumed_policy_short_WRITE_still_rejected",!pm_usb_ssd_command(2)&&fault&&write_calls==1&&flush_calls==0&&fenced());
 header();bool h=pm_usb_ssd_command(2);specs.resize(3);specs[2].len=4;check("assumed_policy_short_READ_still_rejected",h&&!pm_usb_ssd_command(1)&&fault&&captured.empty()&&fenced());
 header();expire_ready_on=1;check("header_expiry_before_query",!pm_usb_ssd_command(2)&&jobs.empty()&&fault&&fenced());
 header();specs[0].delay=2000000;check("header_query_expiry_no_input",!pm_usb_ssd_command(2)&&order({QUERY})&&pio_calls==0&&assumed()==0&&fault&&fenced());
 header();pio_delay=2000000;check("header_PIO_expiry_no_WRITE",!pm_usb_ssd_command(2)&&order({QUERY})&&write_calls==0&&fault&&fenced());
 header();specs.resize(2);specs[0].delay=1200000;specs[1].delay=900000;check("header_WRITE_shared_deadline",!pm_usb_ssd_command(2)&&order({QUERY,WRITE})&&fault&&fenced());
 header();specs[0].never=true;bool failed_before_late=!pm_usb_ssd_command(2);deliver(0);check("late_header_callback_no_reuse",failed_before_late&&fault&&!write_ready&&pio_calls==0&&assumed()==0&&fenced());
 header();bool prepared=pm_usb_ssd_command(2);tuh_msc_umount_cb(1);tuh_msc_mount_cb(1);check("assumed_policy_disconnect_fenced",prepared&&fault&&fenced());
 header();disconnect_during_pio=true;check("header_disconnect_during_input",!pm_usb_ssd_command(2)&&fault&&write_calls==0&&fenced());
 header();fail_pio=1;check("header_partial_input_no_WRITE",!pm_usb_ssd_command(2)&&fault&&write_calls==0&&fenced());
 header();lba_input=128;check("header_out_of_range_no_WRITE",!pm_usb_ssd_command(2)&&fault&&write_calls==0&&fenced());
 header();bool cached=pm_usb_ssd_command(2);cached=cached&&probe()&&pm_usb_ssd_command(1)&&pm_usb_ssd_command(2);check("header_cached_after_read_geometry_buffer_reuse",cached&&write_calls==2&&read_calls==1&&assumed()==1&&!stale_data_bad);

 // Explicit ownership tests exercise callbacks and actual claim/release commands.
 reset();check("empty_claim",pm_usb_ssd_command(4)&&!fault&&!bound&&captured.size()==8&&captured[2]==0);
 tuh_msc_mount_cb(1);check("late_insertion",pm_usb_ssd_command(4)&&bound&&sectors==128);
 check("clean_release",pm_usb_ssd_command(3)&&!bound&&!fault&&!busy);
 tuh_msc_umount_cb(1);check("clean_detach",!disk&&!selected&&!sectors&&!fault&&!write_ready);
 block_count=4096;tuh_msc_mount_cb(1);check("different_capacity_same_address",pm_usb_ssd_command(4)&&bound&&sectors==4096&&captured[2]==0&&captured[3]==16);
 for(unsigned round=0;round<20;round++){
  bool ok=pm_usb_ssd_command(3);tuh_msc_umount_cb(1);block_count=129+round;tuh_msc_mount_cb(1);
  check("repeated_swap_"+std::to_string(round),ok&&pm_usb_ssd_command(4)&&bound&&!fault&&sectors==129+round);
 }
 reset();tuh_msc_mount_cb(1);check("legacy_geometry_rejected",!pm_usb_ssd_command(0)&&fault&&fenced());
 reset();tuh_msc_mount_cb(1);check("unclaimed_read_rejected",!pm_usb_ssd_command(1)&&fault&&!read_calls&&fenced());
 reset();tuh_msc_mount_cb(1);check("unclaimed_write_rejected",!pm_usb_ssd_command(2)&&fault&&!write_calls&&fenced());
 mounted();check("double_claim_quarantines",!pm_usb_ssd_command(4)&&fault&&fenced());
 reset();check("release_without_claim",!pm_usb_ssd_command(3)&&fault&&fenced());
 mounted();ready_flag=false;check("release_not_ready",!pm_usb_ssd_command(3)&&fault&&bound&&fenced());
 mounted();busy=true;check("release_busy",!pm_usb_ssd_command(3)&&bound&&busy);
 mounted();disconnect_during_pio=true;check("removal_during_claim",!probe()&&fault&&fenced());
 mounted();disconnect_during_pio=true;check("removal_during_read_lba",!pm_usb_ssd_command(1)&&fault&&fenced());
 mounted();specs.resize(1);specs[0].never=true;bool timedout=!pm_usb_ssd_command(1);
 tuh_msc_umount_cb(1);tuh_msc_mount_cb(1);deliver(0,true);
 check("late_read_completion_cannot_rebind",timedout&&fault&&bound&&fenced());
 mounted();specs.resize(2);specs[1].never=true;timedout=!pm_usb_ssd_command(2);
 tuh_msc_umount_cb(1);tuh_msc_mount_cb(1);deliver(1,true);
 check("late_write_completion_cannot_rebind",timedout&&fault&&bound&&fenced());
 mounted();mode_reply[6]=4;check("cache_enabled_session",pm_usb_ssd_command(2)&&write_cache_enabled);
 check("release_cache_enabled",pm_usb_ssd_command(3)&&!bound&&!fault);tuh_msc_umount_cb(1);
 check("policy_cleared_on_detach",!write_ready&&!write_cache_enabled);
 block_count=1000;tuh_msc_mount_cb(1);check("new_device_claim",pm_usb_ssd_command(4)&&bound&&sectors==1000&&!write_ready);
 reset();block_size=4096;tuh_msc_mount_cb(1);check("unsupported_claim_no_media",pm_usb_ssd_command(4)&&!bound&&!fault&&captured[2]==0);
 tuh_msc_umount_cb(1);block_size=512;tuh_msc_mount_cb(1);check("unsupported_can_be_replaced",pm_usb_ssd_command(4)&&bound&&!fault);

 printf("\n]\n");fprintf(stderr,"%u tests, %u failures\n",tests,failures);return failures?1:0;
}
