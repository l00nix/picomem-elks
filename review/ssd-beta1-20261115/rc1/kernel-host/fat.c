
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
cluster_t FATPROC fat_access(struct super_block *sb,cluster_t this,cluster_t new_value)
{
	struct buffer_head *bh,*bh2;
	unsigned char *p_first,*p_last;
	void *data,*data2;
	uint32_t *entry;
	cluster_t first,last,next,result;
	sector_t start = MSDOS_SB(sb)->fat_start;
	unsigned int copy = 0;
	int fatsz = MSDOS_SB(sb)->fat_bits;

#ifndef FAT_BITS_32
	if (fatsz == 32)
#endif
		first = last = this << 2;	/* = 4x*/
#ifndef FAT_BITS_32
	else if (fatsz == 16)
		first = last = this << 1;	/* = 2x*/
	else {	/* fat12*/
		first = this + (this >> 1);	/* = 3/2: no multiply for speed*/
		last = first+1;
	}
#endif
next_copy:
	if (!(bh = msdos_sread(sb,
			start+(first >> SECTOR_BITS_SB(sb)),&data))) {
		printk("FAT: bread fat failed\n");
		return 0;
	}
	if ((first >> SECTOR_BITS_SB(sb)) == (last >> SECTOR_BITS_SB(sb))) {
		bh2 = bh;
		data2 = data;
	} else {	/* FAT entry crosses sector boundary, only possible with fat12*/
		if (!(bh2 = msdos_sread(sb,
			start+(last >> SECTOR_BITS_SB(sb)),&data2))) {
			unmap_brelse(bh);
			printk("FAT: bread fat failed\n");
			return 0;
		}
	}
#ifndef FAT_BITS_32
	if (fatsz == 32)
#endif
	{
		entry = &((uint32_t *)data)[(first & (SECTOR_SIZE_SB(sb)-1)) >> 2];
		next = *entry & 0x0fffffffUL;
		if (next >= 0xffffff8L) next = -1;
	}
#ifndef FAT_BITS_32
	else if (fatsz == 16) {
		next = ((unsigned short *)data)[(first & (SECTOR_SIZE_SB(sb)-1)) >> 1];
		if (next >= 0xfff8) next = -1;
	} else {	/* fat12*/
		p_first = &((unsigned char *)data)[first & (SECTOR_SIZE_SB(sb)-1)];
		p_last = &((unsigned char *)data2)[(first+1) & (SECTOR_SIZE_SB(sb)-1)];
		if (this & 1) next = ((*p_first >> 4) | (*p_last << 4)) & 0xfff;
		else next = (*p_first+(*p_last << 8)) & 0xfff;
		if (next >= 0xff8) next = -1;
	}
#endif
	if (!copy) result = next;
	if (new_value != -1) {
#ifndef FAT_BITS_32
		if (fatsz == 32)
#endif
			*entry = (*entry & 0xf0000000UL) |
				((uint32_t)new_value & 0x0fffffffUL);
#ifndef FAT_BITS_32
		else if (fatsz == 16)
			((unsigned short *)data)[(first & (SECTOR_SIZE_SB(sb)-1)) >> 1] =
				(unsigned short)new_value;
		else {	/* fat12*/
			if (this & 1) {
				*p_first = (*p_first & 0xf) | (new_value << 4);
				*p_last = new_value >> 4;
			}
			else {
				*p_first = new_value & 0xff;
				*p_last = (*p_last & 0xf0) | (new_value >> 8);
			}
			if (bh != bh2) debug_fat("fat_access block bh2 dirty %lu\n", buffer_blocknr(bh2));
			mark_buffer_dirty(bh2);
		}
#endif
		debug_fat("fat_access block bh dirty %lu\n", buffer_blocknr(bh));
		mark_buffer_dirty(bh);
	}
	unmap_brelse(bh);
	if (data != data2) unmap_brelse(bh2);
	/* Update each FAT entry without retaining buffers from the previous copy. */
	if (new_value != -1 && ++copy < MSDOS_SB(sb)->fats) {
		start += MSDOS_SB(sb)->fat_length;
		goto next_copy;
	}
	return result;
}

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

int main(void){
 reserved();
 unsigned bitslist[]={12,16,32};unsigned indices[]={2,3,127,128,255,256,341,682};
 for(unsigned f=1;f<=3;f++)for(unsigned k=0;k<3;k++)for(unsigned j=0;j<8;j++){
  unsigned bits=bitslist[k],cluster=indices[j];
  check(bits,f,40000,cluster,-1,0);check(bits,f,40000,cluster,0,0);check(bits,f,40000,cluster,0x123,0);
  unsigned off=bits==12?cluster+cluster/2:cluster*(bits/8),per=(off%512+(bits==32?4:2)>512)?2:1;
  for(unsigned fail=1;fail<=f*per;fail++)check(bits,f,40000,cluster,0x456,fail);
 }
 printf("%u actual-source cases passed\n",cases);
}
