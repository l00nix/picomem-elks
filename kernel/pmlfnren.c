/* SPDX-License-Identifier: GPL-2.0-or-later
 * Native same-directory rename checks. Only creates a NEW test directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
static char directory[96],source[128],dest[128];
static unsigned char data[3073],check[3073];
static void need(int good,const char *what)
{if(!good){fprintf(stderr,"FAIL %s: %s\n",what,strerror(errno));exit(1);}}
static void path(char *out,const char *name)
{strcpy(out,directory);strcat(out,"/");strcat(out,name);}
static void putall(int fd,const unsigned char *p,unsigned n)
{int r;while(n){r=write(fd,(const char*)p,n);need(r>0,"write");p+=r;n-=r;}}
static void verify(const char *name,const unsigned char *p,unsigned len)
{
 int fd,r;unsigned got=0;path(source,name);fd=open(source,O_RDONLY);need(fd>=0,"read open");
 while(got<len){r=read(fd,(char*)check+got,len-got>73?73:len-got);need(r>0,"read");got+=r;}
 need(!memcmp(check,p,len),"contents");need(read(fd,(char*)check,1)==0,"EOF");need(!close(fd),"read close");
}
static void move(const char *a,const char *b,int error)
{int r;path(source,a);path(dest,b);errno=0;r=rename(source,dest);need(error?(r<0&&errno==error):r==0,"rename result");}
int main(int argc,char **argv)
{
 int fd;unsigned j,seen=0;DIR *d;struct dirent *e;struct stat st;
 if(argc!=2||strlen(argv[1])>64||strncmp(argv[1],"/mnt/usb/",9)){
  fprintf(stderr,"Usage: pmlfnren /mnt/usb/NEWTEST\nRetains renamed fixtures in a NEW directory.\n");return 2;
 }
 strcpy(directory,argv[1]);need(!mkdir(directory,0700),"NEW directory (must not exist)");
 for(j=0;j<sizeof(data);j++)data[j]=(j*29+11)&255;
 path(source,"First Long Filename.txt");fd=open(source,O_WRONLY|O_CREAT|O_EXCL,0600);need(fd>=0,"create");
 putall(fd,data,1025);need(!close(fd),"create close");
 move("First Long Filename.txt","Renamed Long Filename.txt",0);
 path(source,"First Long Filename.txt");need(stat(source,&st)<0&&errno==ENOENT,"old name absent");
 verify("Renamed Long Filename.txt",data,1025);
 path(source,"Renamed Long Filename.txt");fd=open(source,O_WRONLY|O_APPEND);need(fd>=0,"append after rename");
 putall(fd,data+1025,2048);need(!close(fd),"append close");verify("Renamed Long Filename.txt",data,sizeof(data));
 move("Renamed Long Filename.txt","renamed long filename.TXT",0);
 d=opendir(directory);need(d!=NULL,"case opendir");while((e=readdir(d)))if(!strcmp(e->d_name,"renamed long filename.TXT"))seen++;
 need(!closedir(d)&&seen==1,"exact case in listing");
 move("renamed long filename.TXT","Renamed Long Filename.txt",0);
 move("Renamed Long Filename.txt","Renamed Long Filename.txt",0);
 path(source,"Keep This File.txt");fd=open(source,O_WRONLY|O_CREAT|O_EXCL,0600);need(fd>=0,"collision fixture");
 putall(fd,(const unsigned char*)"keep",4);need(!close(fd),"collision close");
 move("Renamed Long Filename.txt","Keep This File.txt",EEXIST);
 verify("Keep This File.txt",(const unsigned char*)"keep",4);verify("Renamed Long Filename.txt",data,sizeof(data));
 path(source,"Renamed Long Filename.txt");fd=open(source,O_RDONLY);need(fd>=0,"busy open");
 move("Renamed Long Filename.txt","Busy Rename.txt",EBUSY);need(!close(fd),"busy close");
 path(source,"Keep Directory");need(!mkdir(source,0700),"directory fixture");
 move("Keep Directory","Other Directory",EISDIR);
 move("Renamed Long Filename.txt","Keep Directory/Moved.txt",EXDEV);
 path(source,"Keep Directory");need(!rmdir(source),"remove empty fixture");
 verify("Renamed Long Filename.txt",data,sizeof(data));
 printf("PASS file rename/case-only/no-op; append and content readback\n");
 printf("PASS existing target/open file/directory/cross-parent refusals\n");
 printf("Retained %s: Renamed Long Filename.txt (3073), Keep This File.txt (4)\n",directory);
 return 0;
}
