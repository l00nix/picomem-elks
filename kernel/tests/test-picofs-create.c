#define CONFIG_FS_PICOMEM_LFN_CREATE 1
#include "test-picofs-lfn-write.c"
static void start_create(void)
{start_lfn();create_marker();create_calls=create_status=create_fault=0;}
static void mount_create(void)
{mount_lfn();assert(((struct pm_volume*)usb.u.generic_sbp)->can_create);}
int main(void)
{
 unsigned start=tests,n,j;struct inode *i,*a,*d;struct file f;char data[73];
 existing_lfn_write_tests();start=tests;
 start_lfn();mount_lfn();assert(!create_calls);n=calls;usb.s_mounted->i_count++;
 assert(create_pm(usb.s_mounted,"New Long Filename.txt",21,0600,&i)<0);assert(calls==n);pass();
 start_create();mount_create();i=rw_create(usb.s_mounted,"New Long Filename.txt");
 a=child(usb.s_mounted,"new long filename.TXT");assert(i==a);iput(a);
 struct pm_volume *v=usb.u.generic_sbp;
 a=child(usb.s_mounted,v->nodes[i->i_ino-1].name);assert(i==a);iput(a);
 f=(struct file){2,0,0};assert(write_pm(i,&f,"payload",7)==7);f.f_pos=0;
 assert(read_pm(i,&f,data,7)==7&&!memcmp(data,"payload",7));iput(i);
 n=rw_mutations;usb.s_mounted->i_count++;
 assert(create_pm(usb.s_mounted,"new long filename.txt",21,0600,&i)==-EEXIST);assert(n==rw_mutations);
 assert(!rw_remove(usb.s_mounted,"New Long Filename.txt",0));absent(usb.s_mounted,"New Long Filename.txt");pass();
 start_create();mount_create();assert(!rw_mkdir(usb.s_mounted,"New Long Directory"));
 d=child(usb.s_mounted,"New Long Directory");i=rw_create(d,"12345678901234567890123456");
 v=usb.u.generic_sbp;assert(!strcmp(v->lfn[i->i_ino-1],"12345678901234567890123456"));
 assert(!makepath(v,i->i_ino-1));assert(strstr(path,"NEW")&&!strstr(path,"Long"));
 iput(i);iput(d);
 put_super_pm(&usb);usb.s_dev=DEV_USB;usb.s_flags=0;memset(inodes,0,sizeof(inodes));ninodes=0;mount_create();
 d=child(usb.s_mounted,"New Long Directory");i=child(d,"12345678901234567890123456");iput(i);iput(d);pass();
 start_create();mount_create();n=rw_mutations;
 const char *bad[]={"New Long Filename.txt/",".."," leading",".hidden","trail.","trail ","123456789012345678901234567"};
 for(j=0;j<sizeof(bad)/sizeof(*bad);j++){usb.s_mounted->i_count++;assert(create_pm(usb.s_mounted,bad[j],strlen(bad[j]),0600,&i)<0);}
 assert(n==rw_mutations);pass();
 start_create();rw_add("\\CAF\303\251NOT.TXT",32);mount_create();n=create_calls;usb.s_mounted->i_count++;
 assert(create_pm(usb.s_mounted,"New Long Filename.txt",21,0600,&i)==-EACCES);assert(create_calls==n);pass();
 for(j=1;j<=8;j++)if(j==1||j==2||j==3||j==7||j==8){
  start_create();mount_create();create_status=j;n=rw_mutations;usb.s_mounted->i_count++;
  assert(create_pm(usb.s_mounted,"New Long Filename.txt",21,0600,&i)<0);assert(!broken&&!busy&&rw_mutations==n);pass();
 }
 for(j=1;j<=5;j++){
  start_create();mount_create();create_fault=j;usb.s_mounted->i_count++;
  assert(create_pm(usb.s_mounted,"New Long Filename.txt",21,0600,&i)==-EIO);assert(broken&&!busy);
  n=calls;usb.s_mounted->i_count++;assert(create_pm(usb.s_mounted,"Another Long Name.txt",21,0600,&i)==-EIO);assert(calls==n);pass();
 }
 start_create();mount_create();create_status=6;usb.s_mounted->i_count++;
 assert(create_pm(usb.s_mounted,"New Long Filename.txt",21,0600,&i)==-EIO);assert(broken&&!busy);pass();
 start_create();create_fault=6;assert(!read_super_pm(&usb,NULL,0));assert(broken&&!busy);pass();
 start_create();usb.s_flags=MS_RDONLY;mount_lfn();assert(!create_calls);n=calls;
 usb.s_mounted->i_count++;assert(create_pm(usb.s_mounted,"NEW.TXT",7,0600,&i)==-EROFS);assert(calls==n);pass();
 start_create();mount_create();assert(read_super_pm(&sd,NULL,0));n=create_calls;
 sd.s_mounted->i_count++;assert(create_pm(sd.s_mounted,"NEW.TXT",7,0600,&i)==-EROFS);assert(create_calls==n);pass();
 start_create();mount_create();v=usb.u.generic_sbp;v->used=NODES;n=create_calls;
 usb.s_mounted->i_count++;assert(create_pm(usb.s_mounted,"New Long Filename.txt",21,0600,&i)==-ENOSPC);assert(create_calls==n);pass();
 start_create();mount_create();v=usb.u.generic_sbp;n=create_calls;
 /* A late/ambiguous transport completion must latch and never retry. */
 fault=1;failcmd=0x71;usb.s_mounted->i_count++;
 assert(create_pm(usb.s_mounted,"New Long Filename.txt",21,0600,&i)<0);assert(broken&&!busy);
 j=calls;usb.s_mounted->i_count++;assert(create_pm(usb.s_mounted,"Second Long Name.txt",20,0600,&i)==-EIO);assert(j==calls);pass();
 printf("picofs long-name creation: %u scenarios passed\n",tests-start);return 0;
}
