/* Hardware-free tests of the actual kernel filesystem and transport. */
#define PICOCFS_TEST
#include "../picofs.c"

static unsigned calls, opened, closed, pending, active_cmd, failcmd, fault, next_status;
static unsigned test_port, cursor, directory, drv, tests;
static unsigned count_override, delay;
static unsigned char before[FRAME];
static struct inode inodes[256];
static unsigned ninodes;
static struct super_block usb, sd;
static unsigned char fixture(unsigned long offset) { return (offset*17+3)&255; }
static void le16(unsigned char *p,unsigned v) {p[0]=v;p[1]=v>>8;}
static void le32(unsigned char *p,unsigned long v) {le16(p,v);le16(p+2,v>>16);}
int picomem_mouse_busy(void) {return test_mouse;}
static struct inode *iget(struct super_block *s,unsigned long ino)
{
    unsigned j;
    for(j=0;j<ninodes;j++) if(inodes[j].i_sb==s && inodes[j].i_ino==ino) {
        inodes[j].i_count++; return &inodes[j];
    }
    assert(ninodes<256);
    struct inode *i=&inodes[ninodes++];
    i->i_sb=s;i->i_ino=ino;i->i_count=1;
    s->s_op->read_inode(i); return i;
}
static int do_mount(kdev_t d,char *dir,int type,int flags,char *data)
{ (void)dir;(void)data; assert(type==4);assert(flags==0||flags==1);return d; }
static unsigned rd16(const unsigned char *p) {return p[0]+((unsigned)p[1]<<8);}
static void reply_entry(unsigned char *p,unsigned index)
{
    const char *name;
    unsigned size;
    if(directory) { name="SUB     TXT";size=17; }
    else if(index==0) {name=drv==20?"FIXTURE TXT":"SDTEST  TXT";size=700;}
    else {name="DIR        ";size=0;}
    memset(p,0,24);p[0]=size?32:16;memcpy(p+1,name,11);
    le32(p+16,size);le16(p+20,1);
}
#ifdef PICORW_TEST
#include "rw-fixture.h"
#endif
#ifdef PICOLFN_TEST
#include "lfn-fixture.h"
#endif
#ifdef CONFIG_FS_PICOMEM_LFN_CREATE
#include "create-fixture.h"
#endif
#ifdef CONFIG_FS_PICOMEM_LFN_RENAME
#include "rename-fixture.h"
#endif
static void respond(void)
{
    unsigned char *p=mmio+BASE;
    unsigned cmd=p[59], len=60, ax=0, req=rd16(p+52), n;
    unsigned long offset;
    assert(!irq_enabled);
    drv=p[58];assert(drv==18||drv==20);
#ifdef PICODIAG_TEST
    if(cmd==0x73){respond_diag();return;}
#endif
#ifdef CONFIG_FS_PICOMEM_LFN_RENAME
    if(cmd==0x72){respond_rename();return;}
#endif
#ifdef CONFIG_FS_PICOMEM_LFN_CREATE
    if(cmd==0x71){respond_create();return;}
#endif
#ifdef PICOLFN_TEST
    if(cmd==0x70) {respond_lfn();return;}
#endif
#ifdef PICORW_TEST
    if (rw_active) {respond_rw(cmd,req);return;}
#endif
    if(cmd==0x1b) {
        assert(p[60]==0x37);
        assert(req>=74);
        directory=req>74;
        assert(!memcmp(p+req-12,"????????.???",12));
        cursor=0;
    }
    if(fault && cmd==failcmd && fault==6) {ax=5;}
    else if(cmd==0x1b || cmd==0x1c) {
        n=count_override?count_override:(directory?1:2);
        if(fault==11 && cmd==failcmd)n=0;
        if(cursor>=n) ax=18;
        else {
            reply_entry(p+60,cursor);
            if(count_override) {
                char digits[9];snprintf(digits,sizeof(digits),"F%07u",cursor);
                memcpy(p+61,digits,8);
            }
            cursor++;len=84;
        }
    } else if(cmd==0x16) {
        for(n=60;n<66;n++)assert(!p[n]);
        assert(p[66]=='\\');
        reply_entry(p+60,0);le16(p+80,3);le16(p+82,1);p[84]=0;
        opened++;len=85;
    } else if(cmd==0x08) {
        assert(rd16(p+64)==3);
        n=rd16(p+66);assert(n && n<=CHUNK);
        offset=dword(p+60);assert(offset+n<=700);
        for(unsigned j=0;j<n;j++)p[60+j]=fixture(offset+j);
        len=60+n;
    } else if(cmd==0x06) {assert(rd16(p+60)==3);closed++;}
    else assert(!"non-allowlisted command");
    le16(p+52,len);le16(p+58,ax);
    if(cmd==failcmd) {
        if(fault==3)le16(p+52,65535);
        if(fault==4)p[57]++;
        if(fault==7)le16(p+52,59);
        if(fault==8 && cmd==0x08)le16(p+52,len-1);
        if(fault==9 && cmd==0x16)le16(p+80,16);
        if(fault==10 && cmd==0x16)le32(p+76,701);
    }
}
static unsigned char inb(unsigned port)
{
    if(port==0x2a3)return test_port++;
    assert(port==0x2a0);
    if(pending) {
        if(active_cmd==failcmd && fault==1)return 1;
        if(active_cmd==failcmd && fault==2) {pending=0;return 0;}
        if(active_cmd==failcmd && fault==5)return 3;
        if(delay) {--delay;return 1;}
        pending=0;respond();
    }
    return next_status;
}
static void outb(unsigned char value,unsigned port)
{
    assert(!irq_enabled);
    if(port==0x2a0) {assert(value==0x89);calls++;pending=1;active_cmd=mmio[BASE+59];}
    else assert((port==0x2a1||port==0x2a2)&&!value);
}
static void reset(void)
{
#ifdef PICORW_TEST
    rw_active=0;
#endif
    memset(mmio,0,sizeof(mmio));memset(inodes,0,sizeof(inodes));ninodes=0;
    memset(&usb,0,sizeof(usb));memset(&sd,0,sizeof(sd));
    mounts=busy=broken=sequence=0;
#ifdef CONFIG_FS_PICOMEM_WRITE
    writing=0;
#endif
    calls=opened=closed=pending=active_cmd=failcmd=fault=next_status=0;
    test_port=cursor=directory=drv=count_override=delay=0;
    test_mouse=fail_alloc=copy_fault=0;irq_enabled=1;
    mmio[0]=0x55;mmio[1]=0xaa;mmio[2]=0x20;
    memcpy(mmio+28,"(Date 2026-06-16)",17);
    mmio[0x4000]=0x12;mmio[0x401b]=11;mmio[0x400b]=7;mmio[0x400c]=0xa8;
    mmio[CFG+210]=1;mmio[CFG+215]=5;mmio[CFG+217]=3;
    for(unsigned j=0;j<FRAME;j++)mmio[BASE+j]=j*7;
    mmio[BASE]=0x12;mmio[BASE+1]=0x34;mmio[BASE+2]=0xaa;
    memcpy(before,mmio+BASE,FRAME);memcpy(config,mmio+CFG,768);
    usb.s_dev=DEV_USB;usb.s_flags=MS_RDONLY;
    sd.s_dev=DEV_SD;sd.s_flags=MS_RDONLY;
}
static void same(void) {assert(irq_enabled);assert(!memcmp(before,mmio+BASE,FRAME));}
static void unmount_all(void)
{if(usb.u.generic_sbp)put_super_pm(&usb);if(sd.u.generic_sbp)put_super_pm(&sd);}
static void pass(void) {unmount_all();tests++;}
static struct inode *child(struct inode *dir,const char *name)
{
    struct inode *i=NULL;dir->i_count++;
    assert(lookup(dir,name,strlen(name),&i)==0);assert(i);return i;
}
static char names[16][27];static unsigned nn;
static int fill(char *b,char *n,size_t len,long pos,unsigned long ino)
{(void)b;(void)pos;assert(ino);assert(nn<16);memcpy(names[nn],n,len);names[nn++][len]=0;return 0;}
static int fill_fail(char *b,char *n,size_t len,long pos,unsigned long ino)
{(void)b;(void)n;(void)len;(void)pos;(void)ino;return -EFAULT;}
int main(void)
{
    struct inode *i,*d,*other;
    struct file f;
    char buf[1100];int r;unsigned n;
    reset();assert(read_super_pm(&usb,NULL,0)==&usb);assert(mounts==1);same();
    i=child(usb.s_mounted,"fixture.txt");assert(i->i_size==700);assert(i->i_mode==(S_IFREG|0400));
    f=(struct file){1,0,0};assert(read_pm(i,&f,buf,8)==8); /* response length equals request */
    for(n=0;n<8;n++)assert((unsigned char)buf[n]==fixture(n));
    assert(opened==closed);same();
    assert(read_pm(i,&f,buf,1000)==692);assert(f.f_pos==700);
    for(n=0;n<692;n++)assert((unsigned char)buf[n]==fixture(n+8));
    assert(read_pm(i,&f,buf,10)==0);assert(opened==closed);same();pass();

    reset();assert(read_super_pm(&usb,NULL,0));assert(read_super_pm(&sd,NULL,0));
    assert(mounts==2);assert(child(sd.s_mounted,"sdtest.txt"));
    d=child(usb.s_mounted,"DIR");assert(child(d,"sub.txt"));
    other=child(d,"..");assert(other==usb.s_mounted);
    i=child(usb.s_mounted,"FIXTURE.TXT");directory=0;
    f=(struct file){1,257,0};assert(read_pm(i,&f,buf,7)==7);
    for(n=0;n<7;n++)assert((unsigned char)buf[n]==fixture(n+257));
    same();pass();

    reset();assert(read_super_pm(&usb,NULL,0));nn=0;f=(struct file){1,0,0};
    for(n=0;n<7;n++)assert(!readdir_pm(usb.s_mounted,&f,NULL,fill));
    assert(nn==4);assert(!strcmp(names[0],"."));assert(!strcmp(names[1],".."));
    assert(!strcmp(names[2],"FIXTURE.TXT"));assert(!strcmp(names[3],"DIR"));
    f.f_pos=2;assert(readdir_pm(usb.s_mounted,&f,NULL,fill_fail)==-EFAULT);assert(f.f_pos==2);pass();

    reset();assert(picofs_mount("usb","/mnt/usb",1)==DEV_USB);
    assert(picofs_mount("sd","/mnt/sd",1)==DEV_SD);
    assert(picofs_mount("usbBAD","/mnt",1)==-EINVAL);
    assert(picofs_mount("xx","/mnt",1)==-EINVAL);
    assert(picofs_mount("sd","/mnt",0)==-EROFS);
#ifndef CONFIG_FS_PICOMEM_WRITE
    assert(picofs_mount("usb","/mnt",0)==-EROFS);
#endif
    assert(calls==0);pass();

    reset();assert(read_super_pm(&usb,NULL,0));f=(struct file){2,0,0};
    assert(open_pm(usb.s_mounted,&f)==-EROFS);r=0;assert(remount_pm(&usb,&r,NULL)==-EROFS);
#ifndef CONFIG_FS_PICOMEM_WRITE
    assert(!file_fops.write && !dir_ops.create && !dir_ops.unlink && !dir_ops.mkdir);
#endif
    assert(!super_ops.write_inode && !super_ops.write_super);pass();

    for(n=1;n<=10;n++) {
        reset();assert(read_super_pm(&usb,NULL,0));i=child(usb.s_mounted,"FIXTURE.TXT");
        f=(struct file){1,0,0};failcmd=(n>=9)?0x16:0x08;fault=n;
        assert((int)read_pm(i,&f,buf,8)<0);
        if(n==1||n==2||n==3||n==4||n==5||n==7||n==9)assert(broken);
        if(n==6||n==8||n==10){assert(!broken);assert(opened==closed);same();}
        assert(irq_enabled);unsigned prev=calls;
        if(broken){assert((int)read_pm(i,&f,buf,8)==-EIO);assert(calls==prev);}
        pass();
    }
    reset();fail_alloc=1;assert(!read_super_pm(&usb,NULL,0));assert(!calls&&!busy);pass();
    reset();test_mouse=1;assert(!read_super_pm(&usb,NULL,0));assert(!calls&&!busy);pass();
    reset();busy=1;assert(!read_super_pm(&usb,NULL,0));assert(!calls);busy=0;pass();
    reset();sd.s_flags=0;assert(!read_super_pm(&sd,NULL,0));
#ifndef CONFIG_FS_PICOMEM_WRITE
    usb.s_flags=0;assert(!read_super_pm(&usb,NULL,0));
#endif
    assert(!calls);pass();
    reset();delay=5;assert(read_super_pm(&usb,NULL,0));same();pass();
    reset();count_override=65;assert(!read_super_pm(&usb,NULL,0));assert(!mounts&&!busy);same();pass();
    reset();busy=1;assert(transaction(20,0x09,6,0)==-EROFS);assert(!calls);busy=0;pass();
    reset();busy=1;packet[60]=1;assert(transaction(20,0x16,7,25)==-EROFS);assert(!calls);busy=0;pass();
    reset();busy=1;assert(transaction(17,0x06,2,0)==-EINVAL);assert(!calls);busy=0;pass();
    for(n=0;n<11;n++) {reset();mmio[0x4356+n]=1;assert(!read_super_pm(&usb,NULL,0));assert(!calls);same();pass();}
    const unsigned checks[]={0,1,2,28,0x4000,0x401b,0x400b,0x400c,0x4005,0x4006,0x4007,0x4019,0x401a,BASE,BASE+1,BASE+2,CFG+210,CFG+215,CFG+216,CFG+217,CFG+218,CFG+234,CFG+235};
    for(n=0;n<sizeof(checks)/sizeof(checks[0]);n++) {reset();mmio[checks[n]]^=1;assert(!read_super_pm(&usb,NULL,0));assert(!calls);pass();}
    reset();assert(read_super_pm(&usb,NULL,0));i=child(usb.s_mounted,"FIXTURE.TXT");
    mmio[CFG+1]^=1;f=(struct file){1,0,0};n=calls;
    assert((int)read_pm(i,&f,buf,10)==-ENODEV);assert(calls==n);pass();
    reset();failcmd=0x1b;fault=11;assert(!read_super_pm(&usb,NULL,0));assert(!broken&&!mounts&&!busy);same();pass();
    reset();assert(read_super_pm(&usb,NULL,0));d=child(usb.s_mounted,"DIR");
    failcmd=0x1b;fault=11;nn=0;f=(struct file){1,0,0};
    assert(!readdir_pm(d,&f,NULL,fill));assert(!readdir_pm(d,&f,NULL,fill));
    assert(!readdir_pm(d,&f,NULL,fill));assert(nn==2);pass();
    reset();assert(read_super_pm(&usb,NULL,0));i=child(usb.s_mounted,"FIXTURE.TXT");
    f=(struct file){1,-1,0};n=calls;assert((int)read_pm(i,&f,buf,7)==-EINVAL);assert(n==calls);
    f.f_pos=999;assert(!read_pm(i,&f,buf,7));assert(n==calls);pass();
    reset();assert(read_super_pm(&usb,NULL,0));i=child(usb.s_mounted,"FIXTURE.TXT");
    failcmd=0x06;fault=6;f=(struct file){1,0,0};assert(read_pm(i,&f,buf,8)==8);assert(broken);pass();
    reset();assert(read_super_pm(&usb,NULL,0));i=child(usb.s_mounted,"FIXTURE.TXT");
    copy_fault=1;f=(struct file){1,0,0};assert((int)read_pm(i,&f,buf,8)==-EFAULT);assert(opened==closed);same();pass();
    reset();assert(read_super_pm(&usb,NULL,0));d=child(usb.s_mounted,"DIR");
    count_override=65;unsigned oldused=((struct pm_volume *)usb.u.generic_sbp)->used;
    d->i_count++;assert(lookup(d,"missing",7,&i)==-ENOSPC);
    assert(((struct pm_volume *)usb.u.generic_sbp)->used==oldused);pass();
    reset();char output[13];
    assert(!component(output,(unsigned char *)"ABC     TXT"));assert(!strcmp(output,"ABC.TXT"));
    assert(!component(output,(unsigned char *)"TRASHE~1   "));assert(!strcmp(output,"TRASHE~1"));
    const char *bad[]={"           ","A B     TXT","../        ","X:Y     TXT","A*      TXT","A?      TXT","A\\B     TXT","A.      TXT"};
    for(n=0;n<sizeof(bad)/sizeof(bad[0]);n++)assert(component(output,(const unsigned char *)bad[n])==-EIO);
    pass();
    reset();struct pm_volume v;memset(&v,0,sizeof(v));v.used=12;
    for(n=1;n<12;n++){strcpy(v.nodes[n].name,"ABCDEFGH.TXT");v.nodes[n].parent=n-1;}
    assert(makepath(&v,11)==-ENAMETOOLONG);v.nodes[1].parent=99;assert(makepath(&v,1)==-EIO);pass();
    reset();assert(read_super_pm(&usb,NULL,0));assert(picofs_busy());put_super_pm(&usb);assert(!picofs_busy());
    broken=1;assert(picofs_busy());assert(!read_super_pm(&sd,NULL,0));pass();
    reset();next_status=1;assert(!read_super_pm(&usb,NULL,0));assert(!calls);pass();
    /* Listing must remain available without I/O, even after a transport fault. */
    reset();assert(read_super_pm(&usb,NULL,0));assert(read_super_pm(&sd,NULL,0));
    struct statfs st,expected;memset(&expected,0,sizeof(expected));
    expected.f_blocks=expected.f_bfree=expected.f_bavail=-1L;
    expected.f_files=expected.f_ffree=-1L;
    n=calls;broken=1;busy=1;test_mouse=1;
    memset(&st,0xa5,sizeof(st));usb.s_op->statfs_kern(&usb,&st,1);
    assert(!memcmp(&st,&expected,sizeof(st)));assert(calls==n);same();pass();
    memset(&st,0xa5,sizeof(st));sd.s_op->statfs_kern(&sd,&st,0);
    assert(!memcmp(&st,&expected,sizeof(st)));assert(calls==n);same();pass();
    printf("picofs: %u scenarios passed\n",tests);
    return 0;
}
