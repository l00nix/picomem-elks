#define CONFIG_FS_PICOMEM_LFN_RENAME 1
#define PICO_CREATE_NO_MAIN 1
#include "test-picofs-create.c"
static void start_rename(void)
{start_create();rename_marker();rename_calls=rename_status=rename_fault=0;}
static int ren(struct inode *d,const char *a,const char *b)
{return picofs_rename(d,a,strlen(a),d,b,strlen(b));}
int main(void)
{
 unsigned first,j,n;struct inode *i,*a,*dir;struct file f={0,0,0};char data[73];
 creation_tests();first=tests;
 start_create();mount_create();n=calls;assert(ren(usb.s_mounted,"Long Filename.txt","New Long Filename.txt")==-ENOSYS&&calls==n);pass();
 start_rename();mount_create();i=child(usb.s_mounted,"Long Filename.txt");unsigned long ino=i->i_ino;iput(i);
 assert(!ren(usb.s_mounted,"Long Filename.txt","New Long Filename.txt"));
 a=child(usb.s_mounted,"New Long Filename.txt");assert(a->i_ino==ino);assert(read_pm(a,&f,data,73)==73);iput(a);
 absent(usb.s_mounted,"Long Filename.txt");absent(usb.s_mounted,"FIXTURE.TXT");
 a=child(usb.s_mounted,"RENAM~1.TXT");assert(a->i_ino==ino);iput(a);
 assert(!ren(usb.s_mounted,"New Long Filename.txt","new long filename.TXT"));
 n=rename_calls;assert(!ren(usb.s_mounted,"new long filename.TXT","new long filename.TXT")&&n==rename_calls);
 put_super_pm(&usb);usb.s_dev=DEV_USB;usb.s_flags=0;memset(inodes,0,sizeof(inodes));ninodes=0;mount_create();
 i=child(usb.s_mounted,"new long filename.TXT");iput(i);pass();
 start_rename();mount_create();assert(!ren(usb.s_mounted,"Long Filename.txt","Write After Rename.txt"));
 i=child(usb.s_mounted,"Write After Rename.txt");f=(struct file){2,0,O_APPEND};assert(write_pm(i,&f,"end",3)==3);iput(i);
 i=child(usb.s_mounted,"RENAM~1.TXT");f=(struct file){1,700,0};assert(read_pm(i,&f,data,3)==3&&!memcmp(data,"end",3));iput(i);
 assert(!rw_remove(usb.s_mounted,"Write After Rename.txt",0));absent(usb.s_mounted,"Write After Rename.txt");absent(usb.s_mounted,"RENAM~1.TXT");pass();
 start_rename();mount_create();i=child(usb.s_mounted,"Long Filename.txt");n=rename_calls;
 assert(ren(usb.s_mounted,"Long Filename.txt","Busy Long Name.txt")==-EBUSY&&rename_calls==n);iput(i);pass();
 start_rename();mount_create();assert(ren(usb.s_mounted,"Long Directory","Changed Directory")==-EISDIR);pass();
 start_rename();mount_create();i=rw_create(usb.s_mounted,"Destination Exists.txt");iput(i);n=rw_mutations;
 assert(ren(usb.s_mounted,"Long Filename.txt","Destination Exists.txt")==-EEXIST&&rw_mutations==n);pass();
 start_rename();mount_create();dir=child(usb.s_mounted,"Long Directory");assert(picofs_rename(usb.s_mounted,"Long Filename.txt",17,dir,"Moved.txt",9)==-EXDEV);iput(dir);pass();
 start_rename();mount_create();const char *bad[]={"..",".hidden"," leading","trailing.","slash/name","123456789012345678901234567"};
 for(j=0;j<sizeof(bad)/sizeof(*bad);j++){n=rename_calls;assert(ren(usb.s_mounted,"Long Filename.txt",bad[j])<0&&rename_calls==n);}pass();
 start_rename();rw_add("\\CAF\303\251NOT.TXT",32);mount_create();n=rename_calls;
 assert(ren(usb.s_mounted,"Long Filename.txt","New Long Filename.txt")==-EACCES&&rename_calls==n);pass();
 for(j=1;j<=8;j++)if(j==1||j==2||j==3||j==7||j==8){start_rename();mount_create();rename_status=j;n=rw_mutations;assert(ren(usb.s_mounted,"Long Filename.txt","New Long Filename.txt")<0&&!broken&&!busy&&rw_mutations==n);pass();}
 for(j=1;j<=6;j++)if(j!=5){start_rename();mount_create();rename_fault=j;assert(ren(usb.s_mounted,"Long Filename.txt","New Long Filename.txt")==-EIO&&broken&&!busy);n=calls;assert(ren(usb.s_mounted,"Long Filename.txt","Another New Name.txt")==-EIO&&n==calls);pass();}
 start_rename();rename_fault=5;assert(!read_super_pm(&usb,NULL,0)&&broken&&!busy);pass();
 start_rename();mount_create();rename_status=6;assert(ren(usb.s_mounted,"Long Filename.txt","New Long Filename.txt")==-EIO&&broken);pass();
 start_rename();mount_create();fault=1;failcmd=0x72;assert(ren(usb.s_mounted,"Long Filename.txt","New Long Filename.txt")<0&&broken&&!busy);pass();
 start_rename();usb.s_flags=MS_RDONLY;mount_lfn();n=rename_calls;assert(ren(usb.s_mounted,"Long Filename.txt","New Long Filename.txt")==-EROFS&&rename_calls==n);pass();
 start_rename();mount_create();assert(read_super_pm(&sd,NULL,0));n=rename_calls;assert(ren(sd.s_mounted,"Long Filename.txt","New Long Filename.txt")==-EROFS&&rename_calls==n);pass();
 printf("picofs rename: %u scenarios passed\n",tests-first);return 0;
}
