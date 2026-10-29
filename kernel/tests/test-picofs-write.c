#define CONFIG_FS_PICOMEM_WRITE 1
#define PICORW_TEST 1
#define main readonly_main
#include "test-picofs.c"
#undef main
static void rw_begin(void)
{
    reset();rw_active=1;rw_handle=-1;rw_mkdir_error=rw_lose_close=rw_corrupt_read=rw_overcount=0;
    rw_short=-1;rw_mutations=rw_total=0;rw_cursor=0;rw_close_error=0;
    int n=rw_add("\\FIXTURE.TXT",32);objects[n].size=700;
    for(unsigned j=0;j<700;j++)objects[n].data[j]=fixture(j);
    rw_add("\\DIR",16);usb.s_flags=0;assert(read_super_pm(&usb,NULL,0));
}
static struct inode *rw_create(struct inode *d,const char *name)
{
    struct inode *i=NULL;d->i_count++;
    assert(!create_pm(d,name,strlen(name),0600,&i));assert(i);return i;
}
static int rw_mkdir(struct inode *d,const char *name)
{d->i_count++;return mkdir_pm(d,name,strlen(name),0700);}
static int rw_remove(struct inode *d,const char *name,int directory)
{d->i_count++;return directory?rmdir_pm(d,name,strlen(name)):unlink_pm(d,name,strlen(name));}
#ifdef PICOLFN_TEST
int write_main(void)
#else
int main(void)
#endif
{
    readonly_main();unsigned start=tests,n;
    struct inode *i,*d,*other=NULL;struct file f;char data[1100],back[1100];
    for(n=0;n<sizeof(data);n++)data[n]=(n*31+7)&255;
    rw_begin();i=rw_create(usb.s_mounted,"new.txt");
    assert(i->i_mode==0100600 && !i->i_size);
    f=(struct file){2,0,0};assert(!open_pm(i,&f));
    assert(write_pm(i,&f,data,1024)==1024 && i->i_size==1024 && f.f_pos==1024);
    f.f_pos=0;assert(read_pm(i,&f,back,1024)==1024);assert(!memcmp(data,back,1024));
    assert(opened==closed);same();iput(i);pass();

    rw_begin();i=rw_create(usb.s_mounted,"APPEND.TXT");f=(struct file){2,0,O_APPEND};
    assert(write_pm(i,&f,data,10)==10);f.f_pos=0;assert(write_pm(i,&f,data+10,20)==20);
    assert(i->i_size==30&&f.f_pos==30);f.f_flags=0;f.f_pos=5;
    assert(write_pm(i,&f,"XYZ",3)==3);assert(i->i_size==30);
    f.f_pos=0;assert(read_pm(i,&f,back,30)==30);memcpy(data+5,"XYZ",3);assert(!memcmp(data,back,30));
    assert(!picofs_truncate(i,0));assert(!i->i_size);assert(!objects[rw_find("\\APPEND.TXT")].size);
    assert(picofs_truncate(i,1)==-EINVAL);same();iput(i);pass();

    rw_begin();rw_mkdir_error=1;assert(!rw_mkdir(usb.s_mounted,"RWTEST"));
    d=child(usb.s_mounted,"rwtest");i=rw_create(d,"A.TXT");f=(struct file){2,0,0};
    assert(write_pm(i,&f,"abc",3)==3);assert(rw_remove(d,"A.TXT",0)==-EBUSY);
    iput(i);assert(rw_remove(usb.s_mounted,"RWTEST",1)==-EBUSY);iput(d);
    assert(rw_remove(usb.s_mounted,"RWTEST",1)==-ENOTEMPTY);
    d=child(usb.s_mounted,"RWTEST");assert(!rw_remove(d,"A.TXT",0));
    d->i_count++;assert(lookup(d,"A.TXT",5,&other)==-ENOENT);iput(d);
    assert(!rw_remove(usb.s_mounted,"RWTEST",1));same();pass();

    rw_begin();i=rw_create(usb.s_mounted,"SAME.TXT");iput(i);n=rw_mutations;
    usb.s_mounted->i_count++;assert(create_pm(usb.s_mounted,"same.txt",8,0600,&other)==-EEXIST);
    assert(n==rw_mutations);pass();

    rw_begin();const char *bad[]={"../X","A/B","A*","A?","A.",".X","A B.TXT","TOOLONGXX.TXT","A.TOOLONG"};
    for(n=0;n<sizeof(bad)/sizeof(bad[0]);n++) {
        unsigned prev=rw_mutations;usb.s_mounted->i_count++;
        assert(create_pm(usb.s_mounted,bad[n],strlen(bad[n]),0600,&other)<0);
        assert(prev==rw_mutations);
    }pass();

    rw_begin();i=child(usb.s_mounted,"FIXTURE.TXT");f=(struct file){2,701,0};n=calls;
    assert((int)write_pm(i,&f,data,1)==-EINVAL && calls==n);
    assert(i->i_size==700);iput(i);pass();

    rw_begin();i=child(usb.s_mounted,"FIXTURE.TXT");f=(struct file){2,0,0};
    usb.s_flags|=MS_RDONLY;n=calls;assert((int)write_pm(i,&f,data,1)==-EROFS);
    assert(picofs_truncate(i,0)==-EROFS && calls==n);iput(i);pass();

    rw_begin();assert(read_super_pm(&sd,NULL,0));i=child(sd.s_mounted,"FIXTURE.TXT");
    sd.s_flags=0;((struct pm_volume*)sd.u.generic_sbp)->rw=1;n=calls;f=(struct file){2,0,0};
    assert(open_pm(i,&f)==-EROFS);assert((int)write_pm(i,&f,data,1)==-EROFS);
    assert(picofs_truncate(i,0)==-EROFS);assert(calls==n);iput(i);pass();

    rw_begin();i=rw_create(usb.s_mounted,"SHORT.TXT");f=(struct file){2,0,0};rw_short=7;
    assert(write_pm(i,&f,data,20)==7);assert(i->i_size==7&&!broken);rw_short=0;
    assert((int)write_pm(i,&f,data,20)==-ENOSPC);assert(i->i_size==7&&!broken);iput(i);pass();

    rw_begin();i=rw_create(usb.s_mounted,"LOST.TXT");f=(struct file){2,0,0};rw_lose_close=1;
    assert((int)write_pm(i,&f,data,20)==-EIO);assert(broken&&!i->i_size&&!f.f_pos);
    n=calls;assert((int)write_pm(i,&f,data,20)==-EIO && calls==n);iput(i);pass();

    /* Checked firmware CLOSE returns a real error and retains its handle.
     * Never report the preceding write as committed or issue more commands. */
    const unsigned close_errors[]={6,19,21,31,33};
    for(unsigned e=0;e<sizeof(close_errors)/sizeof(close_errors[0]);e++) {
        rw_begin();i=rw_create(usb.s_mounted,"CLOSEERR.TXT");f=(struct file){2,0,0};
        rw_close_error=close_errors[e];
        assert((int)write_pm(i,&f,data,20)<0&&broken&&!busy&&!writing);
        assert(!i->i_size&&!f.f_pos&&rw_handle>=0);
        n=calls;rw_close_error=0;
        assert((int)write_pm(i,&f,data,20)==-EIO&&calls==n);iput(i);pass();
    }

    rw_begin();i=rw_create(usb.s_mounted,"BAD.TXT");f=(struct file){2,0,0};rw_corrupt_read=1;
    assert((int)write_pm(i,&f,data,20)==-EIO);assert(broken&&!i->i_size);iput(i);pass();

    rw_begin();i=rw_create(usb.s_mounted,"COUNT.TXT");f=(struct file){2,0,0};rw_overcount=1;
    assert((int)write_pm(i,&f,data,20)==-EIO);assert(broken);iput(i);pass();

    rw_begin();i=rw_create(usb.s_mounted,"TIMEOUT.TXT");f=(struct file){2,0,0};failcmd=9;fault=1;
    assert((int)write_pm(i,&f,data,20)==-ETIMEDOUT || broken);assert(broken);
    n=calls;assert((int)write_pm(i,&f,data,20)==-EIO&&calls==n);iput(i);pass();

    rw_begin();i=rw_create(usb.s_mounted,"FAULT.TXT");f=(struct file){2,0,0};copy_fault=1;
    assert((int)write_pm(i,&f,data,20)==-EFAULT);assert(!broken&&!i->i_size&&opened==closed);iput(i);pass();

    rw_begin();i=child(usb.s_mounted,"FIXTURE.TXT");
    ((struct pm_volume*)usb.u.generic_sbp)->nodes[i->i_ino-1].attr|=1;
    f=(struct file){2,0,0};n=calls;assert((int)write_pm(i,&f,data,1)==-EACCES&&n==calls);iput(i);pass();

    rw_begin();n=calls;writing=1;busy=1;memset(packet+60,0,8);putword(packet+64,3);
    assert(transaction(18,9,7,2)==-EROFS && calls==n);
    assert(transaction(20,9,6,2)==-EINVAL && calls==n);
    writing=busy=0;pass();
    printf("picofs USB writes: %u scenarios passed\n",tests-start);
    return 0;
}
