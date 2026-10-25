/* SPDX-License-Identifier: GPL-2.0-or-later
 * Bounded native USB RW tests. l00nix, with AI coding assistance.
 * Requires a NEW directory path; never overwrite an existing test directory.
 * Leaves KEEP.BIN for independent readback/remount/reboot validation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
static char directory[96],file[128];
static unsigned char data[3073],check[3073];
static void fail(const char *what)
{ fprintf(stderr,"FAIL %s: %s\n",what,strerror(errno));exit(1); }
static void need(int good,const char *what)
{ if(!good){fprintf(stderr,"FAIL %s\n",what);exit(1);} }
static void path(const char *name)
{ strcpy(file,directory);strcat(file,"/");strcat(file,name); }
static void putall(int fd,const unsigned char *p,unsigned n)
{
    int r;
    while(n){r=write(fd,(char*)p,n);if(r<=0)fail("write");p+=r;n-=r;}
}
static unsigned long verify(const char *name,const unsigned char *expected,unsigned len)
{
    int fd,r;unsigned got=0;unsigned long hash=2166136261UL;
    path(name);fd=open(file,O_RDONLY);if(fd<0)fail("reopen");
    while(got<len) {r=read(fd,(char*)check+got,len-got>73?73:len-got);if(r<=0)fail("readback");got+=r;}
    need(!memcmp(check,expected,len),"readback content");
    need(read(fd,(char*)check,1)==0,"EOF");if(close(fd))fail("close read");
    for(got=0;got<len;got++){hash^=expected[got];hash*=16777619UL;}
    return hash;
}
int main(int argc,char **argv)
{
    int fd,other,r;unsigned j;DIR *d;struct dirent *e;unsigned seen=0;
    unsigned long hash;
    if(argc!=2 || strlen(argv[1])>sizeof(directory)-1 ||
       strncmp(argv[1],"/mnt/usb/",9)) {
        fprintf(stderr,"Usage: pmrwtest /mnt/usb/NEWTEST\nCreates a new directory, tests writes, leaves KEEP.BIN.\n");return 2;
    }
    strcpy(directory,argv[1]);
    if(mkdir(directory,0700))fail("mkdir NEW directory (must not already exist)");
    for(j=0;j<sizeof(data);j++)data[j]=(j*29+11)&255;
    path("KEEP.BIN");fd=open(file,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)fail("exclusive create");
    putall(fd,data,1025);if(close(fd))fail("close initial");
    other=open(file,O_WRONLY|O_CREAT|O_EXCL,0600);need(other<0&&errno==EEXIST,"exclusive create refuses existing file");
    fd=open(file,O_WRONLY|O_APPEND);if(fd<0)fail("append open");
    need(lseek(fd,0,SEEK_SET)==0,"append seek");putall(fd,data+1025,2048);
    if(close(fd))fail("close append");
    fd=open(file,O_RDWR);if(fd<0)fail("overwrite open");
    need(lseek(fd,249,SEEK_SET)==249,"overwrite seek");
    for(j=249;j<272;j++)data[j]=(unsigned char)(255-j);
    putall(fd,data+249,23);if(close(fd))fail("close overwrite");
    hash=verify("KEEP.BIN",data,sizeof(data));
    printf("PASS create/append/overwrite: %u bytes, FNV1a=%08lx\n",(unsigned)sizeof(data),hash);
    path("TEMP.TXT");fd=open(file,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)fail("create temp");
    putall(fd,(const unsigned char*)"original contents",17);if(close(fd))fail("close temp");
    fd=open(file,O_WRONLY|O_TRUNC);if(fd<0)fail("O_TRUNC");putall(fd,(const unsigned char*)"short",5);if(close(fd))fail("close short");
    verify("TEMP.TXT",(const unsigned char*)"short",5);
    path("TEMP.TXT");fd=open(file,O_RDWR|O_TRUNC);if(fd<0)fail("truncate to empty");
    r=unlink(file);need(r<0&&errno==EBUSY,"unlink open file refused");
    if(close(fd))fail("close empty");
    verify("TEMP.TXT",data,0);
    path("TEMP.TXT");if(unlink(file))fail("unlink");
    fd=open(file,O_RDONLY);need(fd<0&&errno==ENOENT,"deleted file absent");
    path("EMPTY");if(mkdir(file,0700))fail("mkdir child");if(rmdir(file))fail("rmdir child");
    need(rmdir(directory)<0,"nonempty directory retained");
    d=opendir(directory);if(!d)fail("opendir");
    while((e=readdir(d))) if(strcmp(e->d_name,".")&&strcmp(e->d_name,"..")) {
        need(!strcmp(e->d_name,"KEEP.BIN"),"directory cache after deletion");seen++;
    }
    closedir(d);need(seen==1,"one retained fixture");
    printf("PASS truncate/delete/directories; exclusive create and open-file guards\n");
    printf("Retained %s/KEEP.BIN for host/remount readback\n",directory);
    return 0;
}
