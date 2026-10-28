/* SPDX-License-Identifier: GPL-2.0-or-later
 * Native existing-LFN write probe. l00nix, with AI coding assistance.
 * Requires an independent Minix backup matching the current fixture exactly.
 * Leaves modified contents for host/remount checks; restore from the backup.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
static unsigned char original[1024],expected[2049],actual[2049];
static void need(int yes,const char *what)
{if(!yes){fprintf(stderr,"FAIL %s (errno %d); preserve backup for recovery\n",what,errno);exit(1);}}
static unsigned get(const char *path,unsigned char *buf,unsigned cap)
{
    int fd,n;unsigned got=0;char extra;
    fd=open(path,O_RDONLY);need(fd>=0,"read open");
    while(got<cap){n=read(fd,(char *)buf+got,cap-got>73?73:cap-got);need(n>=0,"read");if(!n)break;got+=n;}
    need(read(fd,&extra,1)==0,"bounded file and EOF");need(!close(fd),"read close");return got;
}
static void put(int fd,const unsigned char *p,unsigned n)
{int done;while(n){done=write(fd,(const char *)p,n);need(done>0,"write");p+=done;n-=done;}}
int main(int argc,char **argv)
{
    unsigned len,n,j;int fd,err;unsigned long hash=2166136261UL;
    if(argc==3 && !strcmp(argv[1],"--guards") &&
       !strcmp(argv[2],"/mnt/usb/LFN271A/UNIBAD")) {
        fd=open("/mnt/usb/LFN271A/UNIBAD/NO28.TXT",O_WRONLY|O_CREAT|O_EXCL,0600);
        err=errno;printf("Unsupported-alias create: result=%d errno=%d\n",fd,err);
        need(fd<0&&err==EACCES,"incomplete-directory create denied");
        fd=rmdir(argv[2]);
        err=errno;printf("Unsupported-alias rmdir: result=%d errno=%d\n",fd,err);
        need(fd<0&&err==ENOTEMPTY,"incomplete-directory rmdir denied");
        puts("PASS unsupported-alias creation and rmdir guards");return 0;
    }
    if(argc!=3 || strncmp(argv[1],"/mnt/usb/LFN271A/",17) ||
       strstr(argv[1],"/../") || strncmp(argv[2],"/root/",6)) {
        fprintf(stderr,"Usage: pmlfnrw /mnt/usb/LFN271A/existing-name /root/backup\n");return 2;
    }
    len=get(argv[2],original,sizeof(original));need(len>0,"nonempty fixture backup");
    n=get(argv[1],actual,sizeof(actual));need(n==len&&!memcmp(original,actual,len),"fixture matches backup");
    memcpy(expected,original,len);
    for(j=0;j<1025;j++)expected[len+j]=(j*29+11)&255;
    fd=open(argv[1],O_WRONLY|O_APPEND);need(fd>=0,"append long name");
    need(lseek(fd,0,SEEK_SET)==0,"append seek");put(fd,expected+len,1025);need(!close(fd),"append close");
    fd=open(argv[1],O_RDWR);need(fd>=0,"overwrite long name");
    need(lseek(fd,249,SEEK_SET)==249,"overwrite seek");
    for(j=249;j<272;j++)expected[j]=(255-j)&255;
    put(fd,expected+249,23);need(!close(fd),"overwrite close");
    n=get(argv[1],actual,sizeof(actual));need(n==len+1025&&!memcmp(expected,actual,n),"complete readback");
    for(j=0;j<n;j++){hash^=actual[j];hash*=16777619UL;}
    printf("PASS long-name append/overwrite/readback: %u bytes %08lx\n",n,hash);
    printf("Retained test contents in %s\nRestore from %s after independent verification.\n",argv[1],argv[2]);
    return 0;
}
