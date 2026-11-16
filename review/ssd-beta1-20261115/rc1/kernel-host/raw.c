
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
uint16_t block_read(struct inode *inode, struct file *filp, char *buf, uint16_t count)
{
#if defined(CONFIG_MINIX_FS) || defined(CONFIG_BLK_DEV_CHAR)
    loff_t pos;
    uint16_t chars;
    uint16_t read = 0;

    /* Amount we can do I/O over */
    pos = ((loff_t)inode->i_size) - filp->f_pos;
    if (pos <= 0)
	return 0;       /* EOF */

    if (check_disk_change(inode->i_rdev))
        return -ENXIO;

    if ((loff_t)count > pos) count = (uint16_t)pos;

    while (count > 0) {
	register struct buffer_head *bh;

	/*
	 *      Read the block in
	 */
	/* Reject raw offsets before the block_t conversion can wrap. */
	if (((uint32_t)filp->f_pos >> BLOCK_SIZE_BITS) > (block_t)-1) {
	    if (!read) read = -EFBIG;
	    break;
	}
	chars = (filp->f_pos >> BLOCK_SIZE_BITS);
	if (inode->i_op->getblk) {
	    bh = inode->i_op->getblk(inode, (block_t)chars, 0);
	} else {
	    bh = getblk(inode->i_rdev, (block_t)chars);
	}
	/* Offset to block/offset */
	chars = BLOCK_SIZE - (((uint16_t)(filp->f_pos)) & (BLOCK_SIZE - 1));
	if (chars > count) chars = count;
	if (bh) {
	    if (!readbuf(bh)) {
		if (!read) read = -EIO;
		break;
	    }
	    xms_fmemcpyb(buf, current->t_regs.ds,
		buffer_data(bh) + (((uint16_t)(filp->f_pos)) & (BLOCK_SIZE - 1)),
		buffer_seg(bh), chars);
	    brelse(bh);
	} else fmemsetb(buf, current->t_regs.ds, 0, chars);
	buf += chars;
	filp->f_pos += chars;
	read += chars;
	count -= chars;
    }
#ifdef FIXME
    if (!IS_RDONLY(inode)) inode->i_atime = current_time();
#endif
    return read;
#else
    return -EINVAL;
#endif
}

uint16_t block_write(struct inode *inode, struct file *filp, char *buf, uint16_t count)
{
#if defined(CONFIG_MINIX_FS) || defined(CONFIG_BLK_DEV_CHAR)
    uint16_t chars, offset;
    uint16_t written = 0;

    if (check_disk_change(inode->i_rdev))
        return -ENXIO;

    if (filp->f_flags & O_APPEND) filp->f_pos = (loff_t)inode->i_size;

    while (count > 0) {
	register struct buffer_head *bh;

	/* Reject raw offsets before the block_t conversion can wrap. */
	if (((uint32_t)filp->f_pos >> BLOCK_SIZE_BITS) > (block_t)-1) {
	    if (!written) written = -EFBIG;
	    break;
	}
	chars = (filp->f_pos >> BLOCK_SIZE_BITS);
	if (inode->i_op->getblk) {
	    bh = inode->i_op->getblk(inode, (block_t)chars, 1);
	} else {
	    bh = getblk(inode->i_rdev, (block_t)chars);
	}
	if (!bh) {
	    if (!written) written = -ENOSPC;
	    break;
	}
	/* Offset to block/offset */
	offset = ((uint16_t)filp->f_pos) & (BLOCK_SIZE - 1);
	chars = BLOCK_SIZE - offset;
	if (chars > count) chars = count;
	/*
	 *      Read the block in, unless we
	 *      are writing a whole block.
	 */
	if (chars != BLOCK_SIZE) {
	    if (!readbuf(bh)) {
		if (!written) written = -EIO;
		break;
	    }
	}
	/*
	 *      Alter buffer, mark dirty
	 */
	xms_fmemcpyb(buffer_data(bh) + offset, buffer_seg(bh), buf,
		current->t_regs.ds, chars);
	mark_buffer_uptodate(bh, 1);
	mark_buffer_dirty(bh);
	brelse(bh);
	buf += chars;
	filp->f_pos += chars;
	written += chars;
	count -= chars;
    }
    if ((loff_t)inode->i_size < filp->f_pos)
        inode->i_size = (__u32) filp->f_pos;
    inode->i_mtime = inode->i_ctime = current_time();
    inode->i_dirt = 1;
    return written;
#else
    return -EINVAL;
#endif
}

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
