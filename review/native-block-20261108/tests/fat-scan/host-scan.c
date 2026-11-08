#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
typedef int32_t cluster_t;
struct info {cluster_t clusters,previous_cluster;uint32_t fat_start;int bits,size,sbits;};
struct inode {struct info *i_sb;};
struct buffer_head {int dummy;};
static struct buffer_head buffer;
static union {uint32_t wide[512];uint16_t narrow[1024];} mapped;
static uint32_t *table;
static int active,maps,releases,woken,lock,fail_map;
static cluster_t got_cluster,got_count,got_hint;
#define SECTOR_SIZE_SB(s) ((s)->size)
#define SECTOR_BITS_SB(s) ((s)->sbits)
static void wake_up(int *ignored){(void)ignored;woken++;}
static struct buffer_head *msdos_sread(struct info *sb,uint32_t sector,void **data){
 unsigned n=sb->size/(sb->bits==32?4:2);uint32_t first=(sector-sb->fat_start)*n;
 assert(!active);maps++;if(maps==fail_map)return NULL;
 for(unsigned i=0;i<n;i++){
  uint32_t v=first+i<(uint32_t)sb->clusters+2?table[first+i]:0;
  if(sb->bits==32)mapped.wide[i]=v;else mapped.narrow[i]=(uint16_t)v;
 }
 active=1;*data=&mapped;return &buffer;
}
static void unmap_brelse(struct buffer_head *bh){assert(bh==&buffer && active);active=0;releases++;}
static int fat_access(struct info *sb,cluster_t cl,long v){(void)sb;(void)v;assert(!active);return table[cl]==0?0:1;}
static int scan(struct info *sb){
 struct inode node={sb},*inode=&node;struct buffer_head *bh;
 cluster_t count,this,limit,prev=sb->previous_cluster;int fatsz=sb->bits;int wait=0;
 lock=1;
#include "scan-core.inc"
 got_count=count;got_cluster=count<limit?this:0;got_hint=sb->previous_cluster;lock=0;
 return count<limit?0:-ENOSPC;
}
static unsigned rnd=0x87654321;
static unsigned random32(void){rnd=rnd*1664525u+1013904223u;return rnd;}
static unsigned cases;
static void test(int bits,int size,int n,int hint,int shape,int inject){
 struct info sb={n,hint,19,bits,size,size==512?9:10};
 table=malloc((n+2)*sizeof(*table));assert(table);
 for(int i=0;i<n+2;i++){
  uint32_t v=(random32()%7)+1;
  if(shape==1 && random32()%9==0)v=0;
  if(bits==32)v|=(random32()&15)<<28;
  table[i]=v;
 }
 if(shape==2)table[n+1]=bits==32?0xf0000000u:0;
 if(shape==3)table[2]=0;
 if(shape==4)table[2+(n/2)]=0;
 int initial=hint%n,visits=0,selected=0,oracle_maps=0,lastsector=-1;
 int error=0;
 for(int i=0;i<n;i++){
  int cl=(initial+i)%n+2;
  if(i && cl==2)lastsector=-1;
  int sector=cl/(size/(bits==32?4:2));
  if(bits!=12 && sector!=lastsector){oracle_maps++;lastsector=sector;if(oracle_maps==inject){error=1;break;}}
  visits++;
  if((table[cl]&(bits==32?0x0fffffffu:0xffffu))==0){selected=cl;break;}
 }
 active=maps=releases=woken=0;fail_map=inject;
 int result=scan(&sb);
 assert(!active && !lock);
 if(error){assert(result==-EIO && maps==inject && releases==maps-1 && woken==1);assert(sb.previous_cluster==hint);}
 else{
  assert(result==(selected?0:-ENOSPC));assert(got_cluster==selected);
  assert(got_count==(selected?visits-1:n));assert(got_hint==((selected?visits:n+1)+hint)%n);
  assert(maps==oracle_maps && releases==maps && woken==0);
 }
 free(table);cases++;
}
int main(void){
#ifdef FAT_BITS_32
 const int first_bits=32;
#else
 const int first_bits=12;
#endif
 int ns[]={1,2,127,128,129,255,256,257,511,512,513,65535,65536,77144,131079};
 for(int bits=first_bits;bits<=32;bits=bits==12?16:bits==16?32:33)
  for(int size=512;size<=1024;size*=2)
   for(unsigned k=0;k<sizeof(ns)/sizeof(ns[0]);k++)
    for(int shape=0;shape<5;shape++)
     for(int h=0;h<4;h++){
      int n=ns[k],hint=h==0?0:h==1?n-1:h==2?n/2:n*3+7;
      test(bits,size,n,hint,shape,0);
      if(bits!=12){test(bits,size,n,hint,shape,1);test(bits,size,n,hint,shape,2);}
     }
 printf("PASS %u actual-source sector scans: FAT12/16/32, 512/1024 sectors, wrap/full/high clusters/reserved bits/read errors/balanced release\n",cases);
 return 0;
}
