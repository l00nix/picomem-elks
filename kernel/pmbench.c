/* SPDX-License-Identifier: GPL-2.0-or-later
 * Bounded filesystem workload. The host times READY/go/DONE exchanges.
 * No ELKS tick-based elapsed times: PicoMEM transactions mask interrupts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

static char first[128],second[128],temp[128],directory[128];
static unsigned char data[3073],check[3073];
static unsigned sequence;
static void need(int good,const char *what)
{if(!good){fprintf(stderr,"PMBENCH FAIL %s: %s\n",what,strerror(errno));exit(1);}}
static void putall(int fd,const unsigned char *p,unsigned n)
{int r;while(n){r=write(fd,(const char*)p,n);need(r>0,"write");p+=r;n-=r;}}
static void verify(void)
{
 int fd,r;unsigned got=0;
 fd=open(first,O_RDONLY);need(fd>=0,"read open");
 while(got<sizeof(check)){
  r=read(fd,(char*)check+got,sizeof(check)-got);need(r>0,"read payload");got+=r;
 }
 need(!memcmp(data,check,sizeof(data)),"exact payload");
 need(read(fd,(char*)check,1)==0,"EOF");need(!close(fd),"read close");
}
static void start(const char *op,unsigned count)
{
 char input[16];
 printf("PMBENCH READY %u %s %u\n",++sequence,op,count);fflush(stdout);
 need(fgets(input,sizeof(input),stdin)!=NULL,"host go");
 need(!strcmp(input,"go\n")||!strcmp(input,"go\r\n"),"host protocol");
}
static void done(void)
{printf("PMBENCH DONE %u\n",sequence);fflush(stdout);}
int main(int argc,char **argv)
{
 unsigned j,sample,op,k;int fd;const char *component;
 static const char *names[]={"noop","read3073","write3073","rename_pair","create_unlink","mkdir_rmdir"};
 if(argc!=2||strlen(argv[1])>64)goto usage;
#ifndef PMBENCH_HOST_TEST
 if(strncmp(argv[1],"/mnt/usb/",9))goto usage;
 component=argv[1]+9;
#else
 component=strrchr(argv[1],'/');component=component?component+1:argv[1];
#endif
 if(!*component||strlen(component)>8)goto usage;
 for(j=0;component[j];j++)if(!((component[j]>='A'&&component[j]<='Z')||
    (component[j]>='0'&&component[j]<='9')))goto usage;
 need(!mkdir(argv[1],0700),"NEW directory (must not exist)");
 strcpy(first,argv[1]);strcat(first,"/Runtime File Alpha.txt");
 strcpy(second,argv[1]);strcat(second,"/Runtime File Beta.txt");
 strcpy(temp,argv[1]);strcat(temp,"/Temporary Long File.txt");
 strcpy(directory,argv[1]);strcat(directory,"/Temporary Long Directory");
 for(j=0;j<sizeof(data);j++)data[j]=(j*17+23)&255;
 fd=open(first,O_WRONLY|O_CREAT|O_EXCL,0600);need(fd>=0,"create fixture");
 putall(fd,data,sizeof(data));need(!close(fd),"fixture close");verify();
 for(op=0;op<sizeof(names)/sizeof(names[0]);op++)for(sample=0;sample<3;sample++){
  start(names[op],4);
  for(k=0;k<4;k++)switch(op){
   case 0:break;
   case 1:verify();break;
   case 2:
    fd=open(first,O_WRONLY|O_TRUNC);need(fd>=0,"write open");
    putall(fd,data,sizeof(data));need(!close(fd),"write close");break;
   case 3:need(!rename(first,second),"rename forward");
    need(!rename(second,first),"rename back");break;
   case 4:fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600);need(fd>=0,"create temporary");
    putall(fd,data,73);need(!close(fd),"temporary close");need(!unlink(temp),"unlink");break;
   case 5:need(!mkdir(directory,0700),"mkdir");need(!rmdir(directory),"rmdir");break;
  }
  done();verify();
 }
 printf("PMBENCH PASS 18 batches; retained %s (3073 bytes)\n",first);
 return 0;
 usage:fprintf(stderr,"Usage: pmbench /mnt/usb/NEWTEST\nNew directory: 1-8 uppercase letters/digits. Requires host go handshake.\n");return 2;
}
