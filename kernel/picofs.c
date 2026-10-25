/* SPDX-License-Identifier: GPL-2.0-or-later
 * PicoMEM DFS filesystem for ELKS e7a56a43.
 * l00nix, with AI coding assistance. Experimental PPC512 profile only.
 * Default read-only; optional verified USB writes. SD is always read-only.
 * No block, configuration or USB-reset commands.
 */
#ifdef PICOCFS_TEST
#include "tests/shim.h"
#else
#include <linuxmt/config.h>
#include <linuxmt/types.h>
#include <linuxmt/fs.h>
#include <linuxmt/stat.h>
#include <linuxmt/fcntl.h>
#include <linuxmt/errno.h>
#include <linuxmt/string.h>
#include <linuxmt/sched.h>
#include <linuxmt/kernel.h>
#include <linuxmt/mm.h>
#include <linuxmt/heap.h>
#include <linuxmt/init.h>
#include <linuxmt/kdev_t.h>
#include <arch/io.h>
#include <arch/segment.h>
#include <arch/system.h>
#include <arch/irq.h>
#define MEM ((volatile unsigned char __far *)0xd0000000UL)
#endif

#define BASE 0x4fc4
#define CFG 0x4052
#define CHUNK 256
#define FRAME (60+CHUNK)
#define PATHMAX 128
#define NODES 64
#define POLLS 262144UL
#define DEV_USB MKDEV(0,1)
#define DEV_SD MKDEV(0,2)
#define END_DIR 1

struct pm_node {
    unsigned long size;
    char name[13];
    unsigned char parent, attr, scanned;
};
struct pm_volume {
    unsigned used, drive, rw;
    struct pm_node nodes[NODES];
};
static unsigned char packet[FRAME], saved[FRAME], config[768];
static char path[PATHMAX];
static unsigned char sequence;
static unsigned busy, broken, mounts;
#ifdef CONFIG_FS_PICOMEM_WRITE
static unsigned writing;
#endif
#ifdef CONFIG_MOUSE_PICOMEM
extern int picomem_mouse_busy(void);
#endif

/* Also gates opening/arming the optional mouse driver. A transport fault
 * stays latched until reboot, including after every filesystem is unmounted. */
int picofs_busy(void) { return busy || mounts || broken; }
static int FARPROC enter(void)
{
    if (broken) return -EIO;
    if (busy) return -EBUSY;
#ifdef CONFIG_MOUSE_PICOMEM
    if (picomem_mouse_busy()) return -EBUSY;
#endif
#ifdef CONFIG_FS_PICOMEM_WRITE
    writing = 0;
#endif
    busy = 1;
    return 0;
}
static unsigned FARPROC word(const unsigned char *p)
{ return p[0] + ((unsigned)p[1] << 8); }
static unsigned long FARPROC dword(const unsigned char *p)
{ return word(p) + ((unsigned long)word(p+2) << 16); }
static void FARPROC putword(unsigned char *p, unsigned x)
{ p[0] = x; p[1] = x >> 8; }

/* Called with interrupts disabled. No sleeping, BIOS calls or printing. */
static int FARPROC profile(void)
{
    unsigned i;
    unsigned char p, n;
    static const char date[] = "(Date 2026-06-16)";
    if (MEM[0]!=0x55 || MEM[1]!=0xaa || MEM[2]!=0x20) return -ENODEV;
    for (i=0;i<sizeof(date)-1;i++)
        if (MEM[28+i]!=(unsigned char)date[i]) return -ENODEV;
    if (MEM[0x4000]!=0x12 || MEM[0x401b]!=11 || MEM[0x400b]!=7 ||
        MEM[0x400c]!=0xa8 || MEM[0x4005] || MEM[0x4006] || MEM[0x4007] ||
        MEM[0x4019] || MEM[0x401a] || MEM[BASE]!=0x12 ||
        MEM[BASE+1]!=0x34 || MEM[BASE+2]!=0xaa) return -ENODEV;
    if (MEM[CFG+210]!=1 || MEM[CFG+215]!=5 || MEM[CFG+216] ||
        MEM[CFG+217]!=3 || MEM[CFG+218] || MEM[CFG+234] || MEM[CFG+235])
        return -ENODEV;
    for (i=0;i<11;i++) if (MEM[0x4356+i]) return -EBUSY;
    p=inb(0x2a3);
    for (i=0;i<16;i++) {
        n=inb(0x2a3);
        if (n!=(unsigned char)(p+1)) return -ENODEV;
        p=n;
    }
    return inb(0x2a0) ? -EBUSY : 0;
}
static int FARPROC dos_error(unsigned ax)
{
    switch (ax) {
    case 2: case 3: return -ENOENT;
    case 4: return -EMFILE;
    case 5: return -EACCES;
    case 6: return -EBADF;
    default: return -EIO;
    }
}

/* The sole firmware transport. The caller owns busy and provides only a
 * locally-built payload. Validate the complete allowlist before any I/O.
 * AX changes from drive/opcode to status even when reply length equals the
 * request length (an eight-byte READ is such a case).
 * Preserve disk scratch only after a confirmed, bounded completion. */
static int FARPROC transaction(unsigned drive, unsigned cmd, unsigned req,
                               unsigned response)
{
    unsigned flags, i, len, ax, sentinel;
    unsigned long tries;
    unsigned char status;
    int r;
    if (!busy || broken) return -EIO;
    if ((drive!=20 && drive!=18) || req>CHUNK || response>CHUNK)
        return -EINVAL;
    switch (cmd) {
    case 0x1b: if (req<2 || req>PATHMAX || packet[60]!=0x37 || response!=24) return -EINVAL; break;
    case 0x1c: if (req!=16 || packet[64]!=0x37 || response!=24) return -EINVAL; break;
    case 0x16:
        if (req<7 || req>PATHMAX+6 || response!=25) return -EINVAL;
#ifdef CONFIG_FS_PICOMEM_WRITE
        if (packet[60]==2 && drive==20 && writing) {
            for (i=61;i<66;i++) if (packet[i]) return -EROFS;
            break;
        }
#endif
        for (i=60;i<66;i++) if (packet[i]) return -EROFS;
        break;
    case 0x08:
        if (req!=8 || !response || word(packet+66)!=response ||
            word(packet+64)>=16) return -EINVAL;
        break;
    case 0x06: if (req!=2 || response || word(packet+60)>=16) return -EINVAL; break;
#ifdef CONFIG_FS_PICOMEM_WRITE
    case 0x0f:
        if (!req || req>=PATHMAX || response!=9) return -EINVAL;
        break;
    case 0x09:
        if (drive!=20 || !writing) return -EROFS;
        if (req<7 || response!=2 || word(packet+64)>=16) return -EINVAL;
        break;
    case 0x17: case 0x2e:
        if (drive!=20 || !writing) return -EROFS;
        if (req<7 || req>PATHMAX+6 || response!=25) return -EINVAL;
        for (i=0;i<6;i++)
            if (packet[60+i]!=(cmd==0x2e ? (i==0||i==4?2:i==2?16:0):0))
                return -EINVAL;
        break;
    case 0x03: case 0x01: case 0x13:
        if (drive!=20 || !writing) return -EROFS;
        if (!req || req>=PATHMAX || response) return -EINVAL;
        break;
#endif
    default: return -EROFS;
    }
    memset(packet,0,60);
    packet[0]=0x12; packet[1]=0x34; packet[2]=0xaa;
    packet[12]=0xed; packet[13]=0xf5; packet[56]=1;
    packet[57]=++sequence; packet[58]=drive; packet[59]=cmd;
    putword(packet+52,60+req);
    sentinel=drive+(cmd<<8);
    save_flags(flags); clr_irq();
    r=profile();
    if (r) goto done;
    for (i=0;i<768;i++) if (config[i]!=MEM[CFG+i]) { r=-ENODEV; goto done; }
    for (i=0;i<FRAME;i++) saved[i]=MEM[BASE+i];
    for (i=0;i<60+req;i++) MEM[BASE+i]=packet[i];
    outb(0,0x2a1); outb(0,0x2a2); outb(0x89,0x2a0);
    r=-ETIMEDOUT;
    for (tries=0;tries<POLLS;tries++) {
        status=inb(0x2a0);
        if (status==2 || status==3) { r=-EIO; break; }
        if (status) continue;
        ax=MEM[BASE+58]+((unsigned)MEM[BASE+59]<<8);
        if (ax==sentinel) continue;
        len=MEM[BASE+52]+((unsigned)MEM[BASE+53]<<8);
        /* Never trust length as a copy bound. */
        for (i=0;i<FRAME;i++) packet[i]=MEM[BASE+i];
        for (i=0;i<FRAME;i++) MEM[BASE+i]=saved[i];
        if (len<60 || len>60+response || ax>255 ||
            packet[0]!=0x12 || packet[1]!=0x34 || packet[2]!=0xaa ||
            packet[12]!=0xed || packet[13]!=0xf5 || packet[56]!=1 ||
            packet[57]!=sequence || (ax && len!=60) ||
            (!ax && cmd!=0x08 && len!=60+response)) {
            broken=1; r=-EIO; goto done;
        }
        if (ax==18 && (cmd==0x1b || cmd==0x1c)) { r=END_DIR; goto done; }
        r=ax ? dos_error(ax) : (int)(len-60);
        goto done;
    }
    broken=1; /* No reset, retry or scratch restore on ambiguous completion. */
done:
    restore_flags(flags);
    return r;
}

/* SFNs only. Reject path separators, wildcards, controls and malformed FCBs. */
static int FARPROC component(char *out, const unsigned char *fcb)
{
    unsigned i, j, n=0, end;
    for (j=0;j<2;j++) {
        unsigned start=j?8:0, width=j?3:8;
        end=width;
        while (end && fcb[start+end-1]==' ') --end;
        if (!j && !end) return -EIO;
        if (j && end) out[n++]='.';
        for (i=0;i<end;i++) {
            unsigned char c=fcb[start+i];
            if (c<=32 || c>=127 || strchr("\\/:*?\"<>|.",c)) return -EIO;
            out[n++]=(c>='a' && c<='z') ? c-32 : c;
        }
    }
    out[n]=0;
    return 0;
}
static int FARPROC makepath(struct pm_volume *v, unsigned node)
{
    unsigned n, j, len=0, depth=0;
    path[0]=0;
    while (node) {
        if (node>=v->used || !v->nodes[node].name[0] || ++depth>=NODES) return -EIO;
        n=strlen(v->nodes[node].name);
        if (len+n+1>=PATHMAX) return -ENAMETOOLONG;
        for (j=len+1;j;j--) path[j+n]=path[j-1];
        path[0]='\\'; memcpy(path+1,v->nodes[node].name,n);
        len+=n+1; node=v->nodes[node].parent;
    }
    if (!len) { path[0]='\\'; path[1]=0; }
    return 0;
}

/* Snapshot a complete bounded directory before publishing any of its nodes.
 * The firmware ignores FINDNEXT's position and has one global cursor. No
 * sleeps/callbacks inside this loop: scans on USB and SD cannot interleave.
 * End-of-directory is indistinguishable from a firmware directory error.
 * A zero-entry root is rejected; empty subdirectories remain ambiguous.
 */
static int FARPROC scan(struct pm_volume *v, unsigned node)
{
    unsigned start, req, i, attempts;
    int r;
    struct pm_node *n;
    char name[13];
    if (node>=v->used) return -EIO;
    if (v->nodes[node].scanned) return 0;
    r=makepath(v,node); if (r) return r;
    req=strlen(path);
    if (req && path[req-1]!='\\') path[req++]='\\';
    if (req+12>=PATHMAX) return -ENAMETOOLONG;
    memcpy(path+req,"????????.???",13);
    req+=12;
    packet[60]=0x37; memcpy(packet+61,path,req);
    start=v->used;
    r=transaction(v->drive,0x1b,req+1,24);
    for (attempts=0;attempts<=NODES;attempts++) {
        if (r==END_DIR) {
            if (!node && v->used==start) { r=-ENODEV; break; }
            v->nodes[node].scanned=1;
            return 0;
        }
        if (r<0) break;
        /* FatFs normally omits dot entries; never allow them into paths. */
        if (packet[61]=='.' || (packet[60]&0xc8)) { r=-EIO; break; }
        r=component(name,packet+61); if (r) break;
        for (i=start;i<v->used;i++)
            if (!strcmp(v->nodes[i].name,name)) break;
        if (i!=v->used) { r=-EIO; break; }
        if (v->used==NODES) { r=-ENOSPC; break; }
        n=&v->nodes[v->used++];
        memset(n,0,sizeof(*n));
        strcpy(n->name,name); n->parent=node; n->attr=packet[60];
        n->size=dword(packet+76);
        if (n->size>0x7fffffffUL) { r=-EFBIG; break; }
        memcpy(packet+60,packet+80,4); packet[64]=0x37;
        memset(packet+65,'?',11);
        r=transaction(v->drive,0x1c,16,24);
    }
    v->used=start; /* discard partial snapshot on failure */
    return r<0?r:-EIO;
}

static struct inode_operations dir_ops, file_ops;
static void read_inode(struct inode *i)
{
    struct pm_volume *v=i->i_sb->u.generic_sbp;
    struct pm_node *n;
    if (!i->i_ino || i->i_ino>v->used) return;
    n=&v->nodes[(unsigned)i->i_ino-1];
    /* Private by default, and file execute bits never set. */
    i->i_mode=(n->attr&16) ? S_IFDIR|0500 : S_IFREG|0400;
#ifdef CONFIG_FS_PICOMEM_WRITE
    if (v->rw && !(n->attr&1)) i->i_mode|=0200;
#endif
    i->i_size=n->size; i->i_nlink=1;
    i->i_uid=i->i_gid=0;
    i->i_atime=i->i_mtime=i->i_ctime=0; /* timestamps not translated yet */
    i->i_op=(n->attr&16)?&dir_ops:&file_ops;
}
static int FARPROC lookup_inner(struct inode *dir,const char *name,size_t len,
                               struct inode **result)
{
    struct pm_volume *v=dir->i_sb->u.generic_sbp;
    unsigned node=(unsigned)dir->i_ino-1, i, j;
    char part[13];
    int r;
    if (len>12) return -ENAMETOOLONG;
    if (verified_memcpy_fromfs(part,(void *)name,len)) return -EFAULT;
    part[len]=0;
    if (!len || !strcmp(part,".")) { ++dir->i_count; *result=dir; return 0; }
    if (!strcmp(part,"..")) i=v->nodes[node].parent;
    else {
        for (j=0;j<len;j++) if (part[j]>='a' && part[j]<='z') part[j]-=32;
        r=enter(); if (r) return r;
        r=scan(v,node); busy=0;
        if (r) return r;
        for (i=1;i<v->used;i++)
            if (v->nodes[i].parent==node && !strcmp(v->nodes[i].name,part)) break;
        if (i==v->used) return -ENOENT;
    }
    *result=iget(dir->i_sb,i+1);
    return *result?0:-ENOMEM;
}
static int lookup(struct inode *dir,const char *name,size_t len,struct inode **result)
{
    int r;
    *result=NULL;
    r=lookup_inner(dir,name,len,result);
    iput(dir);
    return r;
}
static int FARPROC readdir_inner(struct inode *i,struct file *f,char *buf,filldir_t fill)
{
    struct pm_volume *v=i->i_sb->u.generic_sbp;
    unsigned node=(unsigned)i->i_ino-1, index;
    int r;
    static char dots[]="..";
    if (f->f_pos<0) return -EINVAL;
    r=enter(); if (r) return r;
    r=scan(v,node); busy=0;
    if (r) return r;
    if (f->f_pos<2) {
        index=f->f_pos ? v->nodes[node].parent : node;
        r=fill(buf,f->f_pos?dots:dots+1,f->f_pos?2:1,f->f_pos,index+1);
        if (!r) ++f->f_pos;
        return r;
    }
    if (f->f_pos>NODES+1) return 0;
    for (index=(unsigned)f->f_pos-1;index<v->used;index++) {
        if (!v->nodes[index].name[0] || v->nodes[index].parent!=node) continue;
        r=fill(buf,v->nodes[index].name,strlen(v->nodes[index].name),f->f_pos,index+1);
        if (!r) f->f_pos=index+2;
        return r;
    }
    f->f_pos=NODES+2;
    return 0;
}
static int readdir_pm(struct inode *i,struct file *f,char *buf,filldir_t fill)
{ return readdir_inner(i,f,buf,fill); }
static int FARPROC read_inner(struct inode *i,struct file *f,char *buf,size_t count)
{
    struct pm_volume *v=i->i_sb->u.generic_sbp;
    unsigned node=(unsigned)i->i_ino-1, fid, n, got=0;
    unsigned long offset;
    int r, close_result;
    if (f->f_pos<0) return -EINVAL;
    if ((unsigned long)f->f_pos>=i->i_size || !count) return 0;
    if (count>1024) count=1024; /* bounded work per read syscall */
    if (count>i->i_size-(unsigned long)f->f_pos) count=i->i_size-f->f_pos;
    r=enter(); if (r) return r;
    r=makepath(v,node); if (r) goto done;
    memset(packet+60,0,6);
    n=strlen(path); memcpy(packet+66,path,n);
    r=transaction(v->drive,0x16,n+6,25);
    if (r<0) goto done;
    fid=word(packet+80);
    if (fid>=16) { broken=1; r=-EIO; goto done; }
    /* Refuse a resized file; equal-size external replacements are undetectable. */
    if (dword(packet+76)!=i->i_size || (packet[60]&0x18) || packet[84]) {
        r=-EIO; goto close;
    }
    while (got<count) {
        n=count-got; if (n>CHUNK) n=CHUNK;
        offset=(unsigned long)f->f_pos+got;
        putword(packet+60,(unsigned)offset); putword(packet+62,(unsigned)(offset>>16));
        putword(packet+64,fid); putword(packet+66,n);
        r=transaction(v->drive,0x08,8,n);
        if (r<0) break;
        if (r!=(int)n) { r=-EIO; break; }
        r=verified_memcpy_tofs(buf+got,packet+60,n);
        if (r) break;
        got+=n;
    }
close:
    if (!broken) {
        putword(packet+60,fid);
        close_result=transaction(v->drive,0x06,2,0);
        if (close_result<0) { broken=1; r=close_result; }
    }
done:
    busy=0;
    if (got) { f->f_pos+=got; return got; }
    return r<0?r:0;
}
static size_t read_pm(struct inode *i,struct file *f,char *buf,size_t count)
{ return read_inner(i,f,buf,count); }
static size_t read_dir(struct inode *i,struct file *f,char *buf,size_t count)
{ (void)i; (void)f; (void)buf; (void)count; return -EISDIR; }
#ifdef CONFIG_FS_PICOMEM_WRITE
#include "picofs-write.inc"
#else
#define write_pm NULL
#define create_pm NULL
#define mkdir_pm NULL
#define unlink_pm NULL
#define rmdir_pm NULL
static int open_pm(struct inode *i,struct file *f)
{ (void)i; return (f->f_mode&FMODE_WRITE)?-EROFS:0; }
#endif
static struct file_operations dir_fops={NULL,read_dir,NULL,readdir_pm,NULL,NULL,open_pm,NULL};
static struct file_operations file_fops={NULL,read_pm,write_pm,NULL,NULL,NULL,open_pm,NULL};
static struct inode_operations dir_ops={&dir_fops,create_pm,lookup,NULL,unlink_pm,NULL,mkdir_pm,rmdir_pm,NULL,NULL,NULL,NULL,NULL};
static struct inode_operations file_ops={&file_fops,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
static int remount_pm(struct super_block *s,int *flags,char *data)
{
    (void)s; (void)data;
#ifdef CONFIG_FS_PICOMEM_WRITE
    if (((struct pm_volume *)s->u.generic_sbp)->rw) return 0;
#endif
    return (*flags&MS_RDONLY)?0:-EROFS;
}
static void put_super_pm(struct super_block *s)
{
    /* Cached VFS inodes may survive iput; invalidate before freeing metadata. */
    invalidate_inodes(s->s_dev);
    heap_free(s->u.generic_sbp); s->u.generic_sbp=NULL;
    s->s_dev=0; --mounts;
}
/* Metadata-only query: never touch firmware or invent media capacity.
 * sys_ustatfs supplies type, device, flags and the actual mountpoint. */
static void statfs_pm(struct super_block *s,struct statfs *buf,int flags)
{
    (void)s; (void)flags;
    memset(buf,0,sizeof(*buf));
    buf->f_blocks=buf->f_bfree=buf->f_bavail=-1L;
    buf->f_files=buf->f_ffree=-1L;
}
static struct super_operations super_ops={read_inode,NULL,NULL,put_super_pm,NULL,remount_pm,statfs_pm};
static struct super_block * FARPROC read_super_inner(struct super_block *s,void *data,int silent)
{
    struct pm_volume *v;
    unsigned flags, j;
    int r;
    (void)data; (void)silent;
    if (s->s_dev!=DEV_USB && s->s_dev!=DEV_SD) return NULL;
    if (!(s->s_flags&MS_RDONLY)) {
#ifdef CONFIG_FS_PICOMEM_WRITE
        if (s->s_dev!=DEV_USB) return NULL;
#else
        return NULL;
#endif
    }
    r=enter(); if (r) return NULL;
    save_flags(flags); clr_irq(); r=profile();
    if (!r && !mounts) for (j=0;j<768;j++) config[j]=MEM[CFG+j];
    restore_flags(flags);
    if (r) { busy=0; return NULL; }
    v=heap_alloc(sizeof(*v),HEAP_TAG_DRVR|HEAP_TAG_CLEAR);
    if (!v) { busy=0; return NULL; }
    v->used=1; v->drive=s->s_dev==DEV_USB?20:18;
    v->rw=!(s->s_flags&MS_RDONLY);
    v->nodes[0].attr=16;
    r=scan(v,0);
    if (r) {
        heap_free(v); busy=0;
        printk("picofs: root query failed %d\n",r);
        return NULL;
    }
    s->u.generic_sbp=v; s->s_op=&super_ops; s->s_flags|=MS_NODEV;
    ++mounts; busy=0;
    s->s_mounted=iget(s,1);
    if (!s->s_mounted) { put_super_pm(s); return NULL; }
    printk("picofs: %s %s, %u root entries\n",v->drive==20?"USB":"SD",v->rw?"read/write":"readonly",v->used-1);
    return s;
}
static struct super_block *read_super_pm(struct super_block *s,void *data,int silent)
{ return read_super_inner(s,data,silent); }
struct file_system_type picofs_fs_type={read_super_pm,FST_PICOFS};

/* Small, explicit VFS callout for a device-less filesystem. No fake disk
 * node or block driver; unnamed dev IDs 0:1 and 0:2 are local reservations. */
int picofs_mount(char *source,char *dir,int flags)
{
    char name[4];
    kdev_t dev;
    int r;
#ifdef CONFIG_FS_PICOMEM_WRITE
    if (flags!=MS_RDONLY && flags) return -EINVAL;
#else
    if (flags!=MS_RDONLY) return -EROFS;
#endif
    r=verified_memcpy_fromfs(name,source,3);
    if (r) return r;
    name[3]=0;
    if (!strcmp(name,"sd")) dev=DEV_SD;
    else {
        if (strcmp(name,"usb")) return -EINVAL;
        r=verified_memcpy_fromfs(name+3,source+3,1);
        if (r || name[3]) return -EINVAL;
        dev=DEV_USB;
    }
    if (dev==DEV_SD && !(flags&MS_RDONLY)) return -EROFS;
    return do_mount(dev,dir,FST_PICOFS,flags,NULL);
}
