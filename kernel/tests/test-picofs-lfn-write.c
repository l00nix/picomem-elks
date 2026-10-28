#define CONFIG_FS_PICOMEM_LFN 1
#define PICOLFN_TEST 1
#include "test-picofs-write.c"

static void start_lfn(void)
{
    rw_begin();put_super_pm(&usb);usb.s_dev=DEV_USB;usb.s_flags=0;
    marker();lfn_calls=lfn_fault=lfn_fallback=0;
    strcpy(objects[0].display,"Long Filename.txt");
    strcpy(objects[1].display,"Long Directory");
}
static void mount_lfn(void)
{assert(read_super_pm(&usb,NULL,0));assert(lfn_calls);}
static void absent(struct inode *d,const char *name)
{struct inode *i=NULL;d->i_count++;assert(lookup(d,name,strlen(name),&i)==-ENOENT);}
#ifdef CONFIG_FS_PICOMEM_LFN_CREATE
int existing_lfn_write_tests(void)
#else
int main(void)
#endif
{
    unsigned start=tests,n,j;struct inode *i,*d,*other;struct file f;char data[600],back[600];
    for(j=0;j<sizeof(data);j++)data[j]=(j*31+7)&255;
    start_lfn();mount_lfn();i=child(usb.s_mounted,"long filename.txt");
    other=child(usb.s_mounted,"FIXTURE.TXT");assert(i==other);iput(other);
    f=(struct file){2,249,0};assert(write_pm(i,&f,data,600)==600);assert(i->i_size==849);
    f.f_pos=249;assert(read_pm(i,&f,back,600)==600);assert(!memcmp(data,back,600));
    f.f_flags=O_APPEND;f.f_pos=0;assert(write_pm(i,&f,"append",6)==6 && i->i_size==855);
    assert(!picofs_truncate(i,0));assert(!i->i_size);same();iput(i);pass();

    start_lfn();n=rw_add("\\DIR\\NESTED~1.TXT",32);strcpy(objects[n].display,"Nested file.txt");
    mount_lfn();d=child(usb.s_mounted,"long directory");i=child(d,"nested FILE.txt");
    f=(struct file){2,0,0};assert(write_pm(i,&f,data,600)==600);
    assert(objects[n].size==600 && !memcmp(objects[n].data,data,600));iput(i);
    assert(!rw_remove(d,"NESTED file.txt",0));absent(d,"Nested file.txt");absent(d,"NESTED~1.TXT");
    i=rw_create(d,"new.txt");iput(i);assert(rw_remove(usb.s_mounted,"Long Directory",1)==-EBUSY);
    assert(!rw_remove(d,"NEW.TXT",0));iput(d);
    assert(!rw_remove(usb.s_mounted,"long directory",1));absent(usb.s_mounted,"Long Directory");pass();

    start_lfn();strcpy(objects[0].display,"OTHER.TXT");mount_lfn();n=rw_mutations;
    usb.s_mounted->i_count++;assert(create_pm(usb.s_mounted,"other.txt",9,0600,&other)==-EEXIST);
    assert(n==rw_mutations);assert(!rw_remove(usb.s_mounted,"other.txt",0));
    absent(usb.s_mounted,"OTHER.TXT");absent(usb.s_mounted,"FIXTURE.TXT");
    i=rw_create(usb.s_mounted,"other.txt");assert(!((struct pm_volume*)usb.u.generic_sbp)->lfn[i->i_ino-1][0]);
    iput(i);pass();

    start_lfn();n=rw_add("\\OTHER.TXT",32);strcpy(objects[n].display,"Long Filename.txt");mount_lfn();
    absent(usb.s_mounted,"Long Filename.txt");n=rw_mutations;
    assert(rw_remove(usb.s_mounted,"Long Filename.txt",0)==-ENOENT);assert(n==rw_mutations);
    assert(!rw_remove(usb.s_mounted,"fixture.txt",0));assert(rw_find("\\OTHER.TXT")>=0);pass();

    start_lfn();rw_add("\\CAF\303\251NOT.TXT",32);mount_lfn();
    assert(((struct pm_volume*)usb.u.generic_sbp)->nodes[0].scanned==2);
    i=child(usb.s_mounted,"Long Filename.txt");f=(struct file){2,0,0};assert(write_pm(i,&f,"ok",2)==2);iput(i);
    n=rw_mutations;usb.s_mounted->i_count++;
    assert(create_pm(usb.s_mounted,"NEW.TXT",7,0600,&other)==-EACCES);
    assert(rw_remove(usb.s_mounted,"Long Filename.txt",0)==-EACCES);assert(n==rw_mutations);pass();

    start_lfn();rw_add("\\DIR\\CAF\303\251NOT.TXT",32);
    n=rw_add("\\DIR\\BADMETA.TXT",34);memcpy(objects[n].fcb,"._CAF\303\251NTXT",11);mount_lfn();
    n=rw_mutations;assert(rw_remove(usb.s_mounted,"Long Directory",1)==-ENOTEMPTY);
    assert(n==rw_mutations);pass();

    /* Exact FCB captured from the real Mac sidecar: prior candidate failed
     * before reaching component(), making ls appear empty while open failed. */
    start_lfn();n=rw_add("\\BADMETA.TXT",34);memcpy(objects[n].fcb,"._CAF\303\251NTXT",11);
    mount_lfn();assert(((struct pm_volume*)usb.u.generic_sbp)->nodes[0].scanned==2);
    i=child(usb.s_mounted,"Long Filename.txt");iput(i);n=rw_mutations;
    usb.s_mounted->i_count++;assert(create_pm(usb.s_mounted,"NEW.TXT",7,0600,&other)==-EACCES);
    assert(n==rw_mutations);pass();

    start_lfn();rw_add("\\CAF\303\251/X.TXT",32);assert(!read_super_pm(&usb,NULL,0));
    assert(!busy&&!mounts);pass();

    start_lfn();objects[0].path[0]=objects[1].path[0]=0;rw_add("\\CAF\303\251NOT.TXT",32);mount_lfn();
    assert(((struct pm_volume*)usb.u.generic_sbp)->used==1);assert(!rw_mutations);pass();

    start_lfn();mount_lfn();n=rw_mutations;
    const char *bad[]={"../Long Filename.txt","Long Filename.txt/","Long*",".","..","New Long Filename.txt"};
    for(j=0;j<sizeof(bad)/sizeof(bad[0]);j++) {
        usb.s_mounted->i_count++;assert(create_pm(usb.s_mounted,bad[j],strlen(bad[j]),0600,&other)<0);
    }assert(n==rw_mutations);pass();

    start_lfn();mount_lfn();i=child(usb.s_mounted,"Long Filename.txt");usb.s_flags=MS_RDONLY;
    f=(struct file){2,0,0};n=rw_mutations;assert((int)write_pm(i,&f,"x",1)==-EROFS);
    assert(picofs_truncate(i,0)==-EROFS);assert(n==rw_mutations);iput(i);pass();

    start_lfn();strcpy(objects[0].display,"12345678901234567890123456");mount_lfn();
    i=child(usb.s_mounted,objects[0].display);f=(struct file){2,0,O_APPEND};
    assert(write_pm(i,&f,"end",3)==3);iput(i);
    put_super_pm(&usb);usb.s_dev=DEV_USB;usb.s_flags=MS_RDONLY;
    /* Model the real VFS invalidation at unmount; the simple shim keeps slots. */
    memset(inodes,0,sizeof(inodes));ninodes=0;mount_lfn();
    i=child(usb.s_mounted,objects[0].display);assert(i->i_size==703);
    f=(struct file){1,700,0};assert(read_pm(i,&f,back,3)==3&&!memcmp(back,"end",3));iput(i);pass();

    start_lfn();mount_lfn();assert(read_super_pm(&sd,NULL,0));i=child(sd.s_mounted,"Long Filename.txt");
    sd.s_flags=0;((struct pm_volume*)sd.u.generic_sbp)->rw=1;
    f=(struct file){2,0,0};n=rw_mutations;
    assert(open_pm(i,&f)==-EROFS);assert((int)write_pm(i,&f,"x",1)==-EROFS);
    assert(picofs_truncate(i,0)==-EROFS);assert(n==rw_mutations);iput(i);pass();

    start_lfn();mount_lfn();i=child(usb.s_mounted,"Long Filename.txt");
    f=(struct file){2,0,0};rw_corrupt_read=1;
    assert((int)write_pm(i,&f,"xyz",3)==-EIO);assert(broken);n=calls;
    assert((int)write_pm(i,&f,"xyz",3)==-EIO&&calls==n);iput(i);pass();
    printf("picofs LFN writes: %u scenarios passed\n",tests-start);return 0;
}
