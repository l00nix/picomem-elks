from pathlib import Path
import subprocess,json,hashlib
p=Path(__file__).resolve().parent
source=(p/'inode.c').read_text();helper=source[source.index('/* FSInfo is advisory.'):source.index('static void print_formatted')]
prefix=r'''
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
typedef uint32_t __u32;
#define MS_RDONLY 1
#define WRITE 1
struct msdos_boot_sector { unsigned char pad[14]; uint16_t reserved; unsigned char pad2[32]; uint16_t info_sector,backup_boot; } __attribute__((packed));
struct sbinfo { int fat_bits; };
struct super_block { int s_flags; struct sbinfo info; };
#define MSDOS_SB(s) (&(s)->info)
struct buffer_head { unsigned char *b_data; int b_uptodate,b_dirty; };
#define EBH(b) (b)
static unsigned char disk[16384],before[16384],cache[1024];
static struct buffer_head bh;
static int held,block,secsize,reads,writes,waits,failread,failwrite,casecount;
static struct buffer_head *msdos_sread(struct super_block *s,unsigned short sector,void **data) {
 (void)s;assert(!held);reads++;if(sector==failread)return NULL;
 assert((unsigned)sector*secsize+512<=sizeof(disk));block=(sector*secsize)/1024;
 memcpy(cache,disk+block*1024,1024);bh.b_data=cache;bh.b_uptodate=1;bh.b_dirty=0;held=1;
 *data=cache+(sector*secsize)%1024;return &bh;
}
static void mark_buffer_dirty(struct buffer_head *b){assert(held&&b==&bh);b->b_dirty=1;}
static void ll_rw_blk(int op,struct buffer_head *b){assert(op==WRITE&&held&&b==&bh&&b->b_dirty);writes++;b->b_dirty=0;b->b_uptodate=(writes!=failwrite);if(b->b_uptodate)memcpy(disk+block*1024,cache,1024);}
static void wait_on_buffer(struct buffer_head *b){assert(held&&b==&bh);waits++;}
static void unmap_brelse(struct buffer_head *b){assert(held&&b==&bh);held=0;}
'''
tests=r'''
static void put16(int off,uint16_t n){memcpy(disk+off,&n,2);}
static void put32(int off,uint32_t n){memcpy(disk+off,&n,4);}
static void info(int sector,uint32_t free,uint32_t next){int o=sector*secsize;put32(o,0x41615252);put32(o+484,0x61417272);put32(o+488,free);put32(o+492,next);put32(o+508,0xaa550000);}
static void setup(int ssize){int i;secsize=ssize;for(i=0;i<(int)sizeof(disk);i++)disk[i]=(unsigned char)(i*17+31);put16(14,16);put16(48,1);put16(50,6);info(1,123,4270);info(7,123,4270);reads=writes=waits=held=0;failread=-1;failwrite=-1;}
static void save(void){memcpy(before,disk,sizeof(disk));}
static void exact(int primary,int backup){int i;for(i=0;i<(int)sizeof(disk);i++){int hint=(primary&&i>=secsize+488&&i<secsize+496)||(backup&&i>=7*secsize+488&&i<7*secsize+496);assert(disk[i]==(hint?255:before[i]));}assert(!held);assert(waits==writes);casecount++;}
int main(void){struct super_block s={0,{32}};int flags,ret,i;
 setup(512);save();assert(!msdos_prepare_rw(&s));assert(writes==2);exact(1,1);
 setup(1024);save();assert(!msdos_prepare_rw(&s));assert(writes==2);exact(1,1);
 setup(512);info(1,~0U,~0U);info(7,~0U,~0U);save();assert(!msdos_prepare_rw(&s));assert(!writes);exact(0,0);
 setup(512);memset(disk+7*512,0,512);save();assert(!msdos_prepare_rw(&s));assert(writes==1);exact(1,0);
 for(i=0;i<3;i++){setup(512);put32(7*512+(i==0?0:i==1?484:508),0);save();assert(!msdos_prepare_rw(&s));assert(writes==1);exact(1,0);}
 setup(512);put32(0+512,0);save();assert(!msdos_prepare_rw(&s));assert(writes==1);exact(0,1);
 for(i=0;i<2;i++){setup(512);put16(48,i?65535:0);save();assert(!msdos_prepare_rw(&s));assert(!writes);exact(0,0);}
 for(i=0;i<2;i++){setup(512);put16(50,i?65535:0);save();assert(!msdos_prepare_rw(&s));assert(writes==1);exact(1,0);}
 setup(512);put16(48,16);save();assert(msdos_prepare_rw(&s)==-EINVAL);assert(!writes);exact(0,0);
 setup(512);put16(50,1);save();assert(msdos_prepare_rw(&s)==-EINVAL);assert(!writes);exact(0,0);
 setup(512);put16(50,16);save();assert(msdos_prepare_rw(&s)==-EINVAL);assert(!writes);exact(0,0);
 setup(512);put16(50,15);save();assert(msdos_prepare_rw(&s)==-EINVAL);assert(!writes);exact(0,0);
 setup(512);put16(14,65535);put16(48,65534);put16(50,65533);save();assert(msdos_prepare_rw(&s)==-EINVAL);assert(!writes);exact(0,0);
 for(i=0;i<3;i++){setup(512);failread=i==0?0:i==1?1:7;save();assert(msdos_prepare_rw(&s)==-EIO);assert(writes==(i==2));exact(i==2,0);}
 for(i=1;i<=2;i++){setup(512);failwrite=i;save();assert(msdos_prepare_rw(&s)==-EIO);assert(writes==i);exact(i==2,0);failwrite=-1;assert(!msdos_prepare_rw(&s));exact(1,1);}
 setup(512);s.info.fat_bits=16;save();assert(!msdos_prepare_rw(&s));assert(!reads&&!writes);exact(0,0);
 setup(512);s.info.fat_bits=12;save();assert(!msdos_prepare_rw(&s));assert(!reads&&!writes);exact(0,0);s.info.fat_bits=32;
 setup(512);s.s_flags=MS_RDONLY;flags=0;save();assert(!msdos_remount(&s,&flags,NULL));assert(s.s_flags==MS_RDONLY&&writes==2);exact(1,1);
 setup(512);s.s_flags=0;flags=MS_RDONLY;save();assert(!msdos_remount(&s,&flags,NULL));assert(!reads&&!writes);exact(0,0);
 setup(512);s.s_flags=MS_RDONLY;flags=MS_RDONLY;save();assert(!msdos_remount(&s,&flags,NULL));assert(!reads&&!writes);exact(0,0);
 setup(512);s.s_flags=0;flags=0;save();assert(!msdos_remount(&s,&flags,NULL));assert(!reads&&!writes);exact(0,0);
 setup(512);s.s_flags=MS_RDONLY;flags=0;failwrite=1;save();ret=msdos_remount(&s,&flags,NULL);assert(ret==-EIO&&s.s_flags==MS_RDONLY);exact(0,0);
 printf("PASS %d FSInfo scenarios\n",casecount);return 0;}
'''
f=p/'host-test.c';f.write_text(prefix+helper+tests)
r=subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined',str(f),'-o',str(p/'host-test')],capture_output=True,text=True);(p/'host-build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
r=subprocess.run([str(p/'host-test')],capture_output=True,text=True);(p/'host-test.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);assert r.returncode==0
(p/'host-result.json').write_text(json.dumps({'passed':True,'result':r.stdout.strip(),'actual_candidate_functions_extracted':True,'sanitizers':['address','undefined'],'source_sha256':hashlib.sha256(source.encode()).hexdigest()},indent=2)+'\n')
