from pathlib import Path
import subprocess,json,hashlib
p=Path(__file__).resolve().parent
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int32_t cluster_t;
typedef uint32_t sector_t;
struct info { uint16_t fat_start,fat_length; uint8_t fats,fat_bits; };
struct super_block { struct info info; };
struct buffer_head { uint32_t block; unsigned refs,dirty; unsigned char data[1024]; };
#define FATPROC
#define MSDOS_SB(s) (&(s)->info)
#define SECTOR_BITS_SB(s) 9
#define SECTOR_SIZE_SB(s) 512
#define debug_fat(...) ((void)0)
#define buffer_blocknr(b) ((b)->block)
static struct buffer_head buffers[16];
static unsigned used,reads,errors,fail_read,peakrefs;
static struct buffer_head *find(uint32_t block) {
 for(unsigned n=0;n<used;n++) if(buffers[n].block==block)return &buffers[n];
 assert(used<16);struct buffer_head *b=&buffers[used++];b->block=block;memset(b->data,0xa5,1024);return b;
}
static struct buffer_head *msdos_sread(struct super_block *s,sector_t sector,void **data) {
 (void)s;reads++;if(reads==fail_read)return NULL;
 struct buffer_head *b=find(sector/2);b->refs++;*data=b->data+(sector%2)*512;
 unsigned refs=0;for(unsigned n=0;n<used;n++)refs+=buffers[n].refs;
 if(refs>peakrefs)peakrefs=refs;return b;
}
static void unmap_brelse(struct buffer_head *b){assert(b->refs);b->refs--;}
static void mark_buffer_dirty(struct buffer_head *b){assert(b->refs);b->dirty++;}
#define printk(...) (errors++)
'''
body=r'''
static void setentry(unsigned char *data,unsigned bits,unsigned off,int odd,uint32_t value){
 if(bits==16){data[off]=value;data[off+1]=value>>8;}
 else if(bits==32){for(unsigned n=0;n<4;n++)data[off+n]=value>>(8*n);}
 else if(odd){data[off]=(data[off]&15)|(value<<4);data[off+1]=value>>4;}
 else{data[off]=value;data[off+1]=(data[off+1]&240)|((value>>8)&15);}
}
static unsigned char *byte_at(uint32_t addr){return find(addr/1024)->data+addr%1024;}
static void put_entry(uint32_t address,unsigned bits,int odd,uint32_t value){
 unsigned char temp[4];unsigned len=bits==32?4:2;
 for(unsigned n=0;n<len;n++)temp[n]=*byte_at(address+n);
 setentry(temp,bits,0,odd,value);
 for(unsigned n=0;n<len;n++)*byte_at(address+n)=temp[n];
}
static unsigned cases;
static void check(unsigned bits,unsigned fats,unsigned length,unsigned cluster,int32_t value,unsigned fail){
 memset(buffers,0,sizeof(buffers));used=reads=errors=peakrefs=0;fail_read=fail;
 struct super_block sb={{1,length,fats,bits}};
 unsigned offset=bits==12?cluster+cluster/2:cluster*(bits/8),len=bits==32?4:2;
 for(unsigned copy=0;copy<fats;copy++)put_entry((1u+copy*length)*512+offset,bits,cluster&1,3+copy);
 struct buffer_head before[16];memcpy(before,buffers,sizeof(buffers));unsigned before_used=used;
 int32_t got=fat_access(&sb,cluster,value);
 unsigned per=((offset%512)+len>512)?2:1;
 unsigned requested=value==-1?1:fats;
 unsigned completed=fail?((fail-1)/per):requested;
 if(fail){assert(got==0&&errors==1&&reads==fail);}
 else{assert(got==3&&errors==0&&reads==per*requested);}
 assert(peakrefs<=2);
 for(unsigned n=0;n<used;n++)assert(buffers[n].refs==0);
 struct buffer_head actual[16];memcpy(actual,buffers,sizeof(buffers));unsigned actual_used=used;
 memcpy(buffers,before,sizeof(buffers));used=before_used;
 if(value!=-1)for(unsigned copy=0;copy<completed;copy++)put_entry((1u+copy*length)*512+offset,bits,cluster&1,(uint32_t)value);
 assert(used==actual_used);
 for(unsigned n=0;n<used;n++)assert(memcmp(buffers[n].data,actual[n].data,1024)==0);
 cases++;
}
int main(void){
 unsigned bitslist[]={12,16,32};unsigned indices[]={2,3,127,128,255,256,341,682};
 for(unsigned f=1;f<=3;f++)for(unsigned k=0;k<3;k++)for(unsigned j=0;j<8;j++){
  unsigned bits=bitslist[k],cluster=indices[j];
  check(bits,f,40000,cluster,-1,0);check(bits,f,40000,cluster,0,0);check(bits,f,40000,cluster,0x123,0);
  unsigned off=bits==12?cluster+cluster/2:cluster*(bits/8),per=(off%512+(bits==32?4:2)>512)?2:1;
  for(unsigned fail=1;fail<=f*per;fail++)check(bits,f,40000,cluster,0x456,fail);
 }
 printf("%u actual-source cases passed\n",cases);
}
'''
s=(p/'fat.c').read_text();function=s[s.index('cluster_t FATPROC fat_access'):s.index('\n\nvoid FATPROC cache_init')]
# ELKS unsigned long is 32 bits; this host is LP64. No control flow transformed.
function=function.replace('unsigned long','uint32_t')
(p/'fat-access-host.c').write_text(prefix+function+body)
r=subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(p/'fat-access-host.c'),'-o',str(p/'fat-access-host')],capture_output=True);(p/'host-build.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr
r=subprocess.run([str(p/'fat-access-host')],capture_output=True);(p/'host-test.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr
(p/'host-result.json').write_text(json.dumps({'passed':True,'source_sha256':hashlib.sha256(s.encode()).hexdigest(),'output':r.stdout.decode(),'host_adaptation':'unsigned long replaced with uint32_t to model IA16 width on LP64; actual function control flow unchanged','asan_ubsan':True,'hardware_access':False},indent=2)+'\n');print(r.stdout.decode())
