from pathlib import Path
import subprocess,json,hashlib,sys
p=Path(__file__).resolve().parent;src=Path(sys.argv[1]).resolve()/'elks/fs';d=p/'kernel-host';d.mkdir(exist_ok=True)
# Retain actual-source mirror/error regressions and add high-nibble cases per copy.
s=(p/'test-inputs/fat-test-template.py').read_text();prefix=s[s.index("prefix=r'''")+11:s.index("'''\nbody=")];body=s[s.index("body=r'''")+9:s.index("'''\ns=(p/")]
extra=r'''
static void reserved(void) {
 for(unsigned nibble=0;nibble<16;nibble++)for(unsigned op=0;op<4;op++) {
  memset(buffers,0,sizeof(buffers));used=reads=errors=peakrefs=0;fail_read=0;
  struct super_block sb={{1,40000,2,32}};
  uint32_t values[]={0,4,0x0fffffff,0x0ffffff8};
  for(unsigned copy=0;copy<2;copy++)put_entry((1u+copy*40000)*512+8,32,0,((nibble+copy)%16u<<28)|3);
  assert(fat_access(&sb,2,-1)==3);
  assert(fat_access(&sb,2,values[op])==3);
  for(unsigned copy=0;copy<2;copy++) {
   uint32_t got;memcpy(&got,byte_at((1u+copy*40000)*512+8),4);
   assert(got==(((nibble+copy)%16u<<28)|values[op]));
  }
  assert(fat_access(&sb,2,-1)==(op>=2?-1:(int32_t)values[op]));
  for(unsigned n=0;n<used;n++)assert(!buffers[n].refs);
  cases++;
 }
}
'''
body=body.replace('int main(void){',extra+'\nint main(void){\n reserved();')
source=(src/'msdos/fat.c').read_text();fn=source[source.index('cluster_t FATPROC fat_access'):source.index('\n\nvoid FATPROC cache_init')].replace('unsigned long','uint32_t')
(d/'fat.c').write_text(prefix+fn+body)
# Compile actual raw block functions with IA16-sized counters and mocked buffers.
raw=(src/'block_dev.c').read_text();fn=raw[raw.index('size_t block_read'):raw.index('\n#if UNUSED')].replace('unsigned long','uint32_t').replace('size_t','uint16_t')
pre=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#define CONFIG_MINIX_FS 1
#define BLOCK_SIZE_BITS 10
#define BLOCK_SIZE 1024
#define O_APPEND 8
typedef int32_t loff_t;
typedef uint16_t block_t;
typedef uint32_t __u32;
struct inode;
struct buffer_head { unsigned char data[1024]; };
struct ops { struct buffer_head *(*getblk)(struct inode*,block_t,int); };
struct inode { uint32_t i_size;int i_rdev,i_mtime,i_ctime,i_dirt;struct ops *i_op; };
struct file { int32_t f_pos;int f_flags; };
static struct { struct { int ds; } t_regs; } task,*current=&task;
static unsigned calls,lastblock,dirty,refs,change;
static struct buffer_head bh;
static int check_disk_change(int dev){(void)dev;return change;}
static struct buffer_head *getblk(int dev,block_t block){(void)dev;calls++;lastblock=block;refs++;return &bh;}
static int readbuf(struct buffer_head *b){assert(b==&bh);return 1;}
static void brelse(struct buffer_head *b){assert(b==&bh&&refs);refs--;}
#define buffer_data(b) ((b)->data)
#define buffer_seg(b) 0
static void xms_fmemcpyb(void *to,int ts,const void *from,int fs,unsigned n){(void)ts;(void)fs;memcpy(to,from,n);}
static void fmemsetb(void *p,int s,int v,unsigned n){(void)s;memset(p,v,n);}
static void mark_buffer_uptodate(struct buffer_head *b,int v){(void)b;(void)v;}
static void mark_buffer_dirty(struct buffer_head *b){assert(b==&bh);dirty++;}
static int current_time(void){return 0;}
'''
post=r'''
int main(void) {
 const int32_t boundary=67108864;
 int32_t offsets[]={0,1023,65535,boundary-1024,boundary-1,boundary,boundary+1024,-1};
 unsigned counts[]={0,1,2,1024,2048};unsigned cases=0;
 for(unsigned wr=0;wr<2;wr++)for(unsigned n=0;n<8;n++)for(unsigned c=0;c<5;c++){
  struct ops op={0};struct inode inode={134217727,1,0,0,0,&op};struct file f={offsets[n],0};char buf[2048]={0};
  calls=dirty=refs=change=0;memset(&bh,0,sizeof(bh));
  uint16_t got=wr?block_write(&inode,&f,buf,counts[c]):block_read(&inode,&f,buf,counts[c]);
  if(!counts[c]){assert(!got&&!calls);}
  else if(offsets[n]<0||offsets[n]>=boundary){assert(got==(uint16_t)-EFBIG&&!calls&&!dirty&&f.f_pos==offsets[n]);}
  else {unsigned available=boundary-offsets[n];unsigned want=counts[c]<available?counts[c]:available;assert(got==want&&f.f_pos==offsets[n]+(int32_t)want);assert(calls&&lastblock<65536&&refs==0);if(f.f_pos==boundary)assert(lastblock==65535);}
  cases++;
 }
 printf("%u raw block boundary cases passed\n",cases);
}
'''
(d/'raw.c').write_text(pre+fn+post)
results={}
for name in ['fat','raw']:
 r=subprocess.run(['clang','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(d/(name+'.c')),'-o',str(d/name)],capture_output=True);(d/(name+'-build.log')).write_bytes(r.stdout+r.stderr);assert not r.returncode,r.stderr
 r=subprocess.run([str(d/name)],capture_output=True);(d/(name+'-test.log')).write_bytes(r.stdout+r.stderr);assert not r.returncode,r.stderr;results[name]=r.stdout.decode();print(r.stdout.decode(),end='')
fs=d/'fsinfo';fs.mkdir(exist_ok=True);(fs/'inode.c').write_bytes((src/'msdos/inode.c').read_bytes());(fs/'host-tests.py').write_bytes((p/'test-inputs/fsinfo-tests.py').read_bytes())
r=subprocess.run([sys.executable,str(fs/'host-tests.py')],capture_output=True);(fs/'run.log').write_bytes(r.stdout+r.stderr);assert not r.returncode,r.stderr;results['fsinfo']=json.loads((fs/'host-result.json').read_text())
results['source_sha256']={str(x.relative_to(src)):hashlib.sha256(x.read_bytes()).hexdigest() for x in src.rglob('*.c')};results['host_model']='Actual functions; size_t u16 and unsigned long u32 adaptations for target widths, mocked storage; ASan/UBSan.'
(d/'results.json').write_text(json.dumps(results,indent=2)+'\n')
