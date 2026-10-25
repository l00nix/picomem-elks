#ifndef PICOFS_SHIM
#define PICOFS_SHIM
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#define FARPROC
#define CONFIG_MOUSE_PICOMEM 1
#define FST_PICOFS 4
#define MS_RDONLY 1
#define MS_NODEV 4
#define FMODE_WRITE 2
#define HEAP_TAG_DRVR 2
#define HEAP_TAG_CLEAR 64
#define MKDEV(a,b) (((a)<<8)|(b))
typedef unsigned kdev_t;
struct inode; struct super_block; struct file;
typedef int (*filldir_t)(char *,char *,size_t,long,unsigned long);
struct file_operations {
 int (*lseek)();
 size_t (*read)(struct inode *,struct file *,char *,size_t);
 size_t (*write)(struct inode *,struct file *,char *,size_t);
 int (*readdir)(); int (*select)(); int (*ioctl)(); int (*open)(); void (*release)();
};
struct inode_operations {
 struct file_operations *default_file_ops;
 int (*create)(struct inode *,const char *,size_t,mode_t,struct inode **); int (*lookup)(struct inode *,const char *,size_t,struct inode **);
 int (*link)(); int (*unlink)(); int (*symlink)(); int (*mkdir)(struct inode *,const char *,size_t,mode_t); int (*rmdir)();
 int (*mknod)(); int (*readlink)(); int (*follow_link)(); void *(*getblk)(); void (*truncate)();
};
struct statfs {
 int f_type; unsigned short f_flags; kdev_t f_dev;
 long f_bsize,f_blocks,f_bfree,f_bavail,f_files,f_ffree;
 char f_mntonname[32];
};
struct super_operations {
 void (*read_inode)(struct inode *); void (*write_inode)(); void (*put_inode)();
 void (*put_super)(); void (*write_super)(); int (*remount_fs)(); void (*statfs_kern)(struct super_block *,struct statfs *,int);
};
struct file_system_type { struct super_block *(*read_super)(); int type; };
struct super_block {
 kdev_t s_dev; unsigned s_flags;
 union {void *generic_sbp;} u;
 struct super_operations *s_op; struct inode *s_mounted;
};
struct inode {
 unsigned long i_ino,i_size,i_mtime,i_atime,i_ctime;
 unsigned i_mode,i_nlink,i_uid,i_gid,i_count;
 struct super_block *i_sb;
 struct inode_operations *i_op;
};
struct file {unsigned f_mode; long f_pos; unsigned f_flags;};
static unsigned char mmio[65536];
#define MEM mmio
static unsigned irq_enabled=1, test_mouse, fail_alloc, copy_fault;
#define save_flags(f) ((f)=irq_enabled)
#define clr_irq() (irq_enabled=0)
#define restore_flags(f) (irq_enabled=(f))
static unsigned char inb(unsigned);
static void outb(unsigned char,unsigned);
static void *heap_alloc(unsigned n,unsigned tag) { (void)tag; return fail_alloc?NULL:calloc(1,n); }
static void heap_free(void *p) { free(p); }
static void invalidate_inodes(kdev_t d) {(void)d;}
static int verified_memcpy_fromfs(void *d,const void *s,unsigned n) { if(copy_fault)return -EFAULT; memcpy(d,s,n); return 0; }
static int verified_memcpy_tofs(void *d,const void *s,unsigned n) { if(copy_fault)return -EFAULT; memcpy(d,s,n); return 0; }
static struct inode *iget(struct super_block *,unsigned long);
static void iput(struct inode *i) { assert(i->i_count); --i->i_count; }
static int do_mount(kdev_t,char *,int,int,char *);
#define printk(...) ((void)0)
#endif
