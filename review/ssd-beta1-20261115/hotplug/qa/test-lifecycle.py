from pathlib import Path
import hashlib,json,subprocess,sys
p=Path(__file__).resolve().parent
src=Path(sys.argv[1]).resolve()/'elks/arch/i86/drivers/block/ssd.c'
s=src.read_text()
def fn(signature):
    a=s.index(signature+'\n{'); b=s.index('{',a); depth=1; c=b+1
    while depth:
        depth+=(s[c]=='{')-(s[c]=='}');c+=1
    return s[a:c]
body='\n\n'.join(fn(x) for x in ['static int ssd_open(struct inode *inode, struct file *filp)','static void ssd_release(struct inode *inode, struct file *filp)','static void do_ssd_request(void)'])
(p/'actual-functions.inc').write_text(body+'\n')
header=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef uint32_t sector_t;
typedef unsigned kdev_t;
struct inode {kdev_t i_rdev; uint32_t i_size;}; struct file {int dummy;};
struct request {int dummy;};
static struct request request_storage,*current;
#define CURRENT current
#define CHECK_REQUEST(x) ((void)(x))
#define debug_blk(...) ((void)0)
#define printk(...) ((void)0)
static sector_t ssd_num_sects;
static char ssd_initialized;
static int access_count;
#ifdef CONFIG_BLK_DEV_SSD_PICOMEM
static char closing;
#endif
static int init_calls,release_calls,flush_calls,inode_calls,buffer_calls,io_calls,end_failures;
static int bound,failed,fail_during_sync,fail_during_release,reentry_checks,check_reentry;
static int dirty,persisted,old_media_writes,new_media_writes,medium,invalidations;
static sector_t available;
static char order[64];static unsigned order_n,checks;
static struct inode inode={0x200,0}, other_inode={0x200,0};
static int ssd_open(struct inode*,struct file*);
static void do_ssd_request(void);
#define T(x) do {assert(x);checks++;} while(0)
static void event(char c) {assert(order_n+1<sizeof(order));order[order_n++]=c;order[order_n]=0;}
static void reenter(void) {
#ifdef CONFIG_BLK_DEV_SSD_PICOMEM
 if(check_reentry){int n=init_calls;T(ssd_open(&other_inode,0)==-EBUSY);T(init_calls==n);reentry_checks++;}
#endif
}
static sector_t ssddev_init(void){init_calls++;if(failed)return 0;bound=available!=0;ssd_initialized=bound;return available;}
static void ssddev_release(void){event('R');release_calls++;reenter();if(fail_during_release)failed=1;if(!failed)bound=0;}
static void ssd_io_complete(void){io_calls++;if(bound&&!failed){persisted++;if(medium==1)old_media_writes++;else new_media_writes++;}current=0;}
static void end_request(int ok){assert(!ok);end_failures++;current=0;}
static void fsync_dev(kdev_t dev){assert(dev==0x200);event('S');flush_calls++;reenter();if(fail_during_sync)failed=1;if(dirty){current=&request_storage;do_ssd_request();dirty=0;}}
static void invalidate_inodes(kdev_t dev){assert(dev==0x200);event('I');inode_calls++;reenter();}
static void invalidate_buffers(kdev_t dev){assert(dev==0x200);event('B');buffer_calls++;invalidations++;dirty=0;reenter();}
'''
tests=r'''
static void reset(void){
 ssd_num_sects=0;ssd_initialized=0;access_count=0;
#ifdef CONFIG_BLK_DEV_SSD_PICOMEM
 closing=0;
#endif
 init_calls=release_calls=flush_calls=inode_calls=buffer_calls=io_calls=end_failures=0;
 bound=failed=fail_during_sync=fail_during_release=reentry_checks=check_reentry=0;
 dirty=persisted=old_media_writes=new_media_writes=invalidations=0;
 available=0;medium=1;order_n=0;order[0]=0;current=0;inode.i_size=0;
}
int main(void){
#ifdef CONFIG_BLK_DEV_SSD_PICOMEM
 reset();T(ssd_open(&inode,0)==-ENODATA);T(access_count==0);T(init_calls==1);T(!failed&&!bound&&!ssd_initialized);
 available=1234;T(ssd_open(&inode,0)==0);T(init_calls==2&&access_count==1);T(inode.i_size==1234u*512);T(ssd_initialized&&bound);
 T(ssd_open(&other_inode,0)==0);T(init_calls==2&&access_count==2);
 ssd_release(&inode,0);T(access_count==1);T(!flush_calls&&!release_calls&&bound);
 dirty=1;check_reentry=1;ssd_release(&other_inode,0);
 T(access_count==0&&!closing&&!bound&&!ssd_initialized&&!ssd_num_sects);T(strcmp(order,"SIBR")==0);T(reentry_checks==4);T(persisted==1&&old_media_writes==1);T(invalidations==1);
 check_reentry=0;fsync_dev(0x200);T(persisted==1); /* sys_umount's extra sync: no dirty buffers */
 current=&request_storage;do_ssd_request();T(end_failures==1&&persisted==1); /* late request fenced while unbound */
 available=777;medium=2;T(ssd_open(&inode,0)==0);T(inode.i_size==777u*512&&init_calls==3);fsync_dev(0x200);T(new_media_writes==0); /* no old cached writes */
 dirty=1;ssd_release(&inode,0);T(new_media_writes==1&&old_media_writes==1);T(!bound);
 reset();available=4096;T(ssd_open(&inode,0)==0);fail_during_sync=1;dirty=1;ssd_release(&inode,0);T(failed&&bound&&!ssd_initialized&&!ssd_num_sects);available=8192;T(ssd_open(&inode,0)==-ENODATA);T(access_count==0&&bound);T(new_media_writes==0);
 reset();available=4096;T(ssd_open(&inode,0)==0);fail_during_release=1;ssd_release(&inode,0);T(failed&&bound);T(ssd_open(&inode,0)==-ENODATA);T(access_count==0);
 reset();available=4096;T(ssd_open(&inode,0)==0);ssd_release(&inode,0);available=0;T(ssd_open(&inode,0)==-ENODATA);T(!failed&&access_count==0);available=9000;T(ssd_open(&inode,0)==0);T(ssd_num_sects==9000);
 reset();available=0x400000;T(ssd_open(&inode,0)==0);T(inode.i_size==0x7ffffff); /* preserve inherited cap literal, not correct it here */
#else
 reset();ssd_num_sects=1000;ssd_initialized=1;bound=1;T(ssd_open(&inode,0)==0);T(init_calls==0&&access_count==1);ssd_release(&inode,0);T(strcmp(order,"SIB")==0);T(!release_calls&&ssd_num_sects==1000&&ssd_initialized);
 reset();
#ifdef CONFIG_FS_XMS
 T(ssd_open(&inode,0)==0);T(access_count==1);
#else
 T(ssd_open(&inode,0)==-ENODATA);T(access_count==0);
#endif
 T(init_calls==0&&release_calls==0);
#endif
 printf("PASS %u checks\n",checks);return 0;
}
'''
(p/'test-lifecycle.c').write_text(header+'\n#include "actual-functions.inc"\n'+tests)
results=[]
for pm,xms in [(1,0),(1,1),(0,0),(0,1)]:
    name=f'pm{pm}-xms{xms}'
    args=['clang','-std=c11','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Wno-unused-parameter','-Wno-unused-function','-Wno-unused-variable']
    if pm:args+=['-DCONFIG_BLK_DEV_SSD_PICOMEM=1']
    if xms:args+=['-DCONFIG_FS_XMS=1']
    args +=[str(p/'test-lifecycle.c'),'-o',str(p/name)]
    build=subprocess.run(args,capture_output=True,text=True);(p/(name+'-build.log')).write_text(build.stdout+build.stderr);assert build.returncode==0,build.stderr
    run=subprocess.run([str(p/name)],capture_output=True,text=True);(p/(name+'.log')).write_text(run.stdout+run.stderr);assert run.returncode==0,run.stdout+run.stderr
    results.append(dict(configuration=name,result=run.stdout.strip(),passed=True))
report=dict(source=str(src),source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),actual_functions=['ssd_open','ssd_release','do_ssd_request'],variants=results,limitations=['Mocked sync/cache/subdriver policy: verifies call order and lifecycle guard, not actual ELKS cache quiescence or USB transport.','Native 16-bit code generation and actual firmware protocol require separate tests.','Post-release mock assumes normal invalidation leaves no referenced dirty buffers; faulted sessions remain quarantined.'])
(p/'lifecycle-result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
