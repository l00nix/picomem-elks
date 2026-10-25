/* SPDX-License-Identifier: GPL-2.0-or-later
 * Read-only native filesystem smoke test. l00nix, with AI coding assistance.
 * Compare differently sized reads, seek/EOF and refusal of a write-only open.
 * No write, truncate or create call is present.
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
static unsigned char a[257],b[257];
int main(int argc,char **argv)
{
    int x,y,n,m,z;unsigned i,total=0;unsigned long hash=2166136261UL;
    struct stat st;
    if(argc!=2){fprintf(stderr,"Usage: pmfstest FILE (up to 32 KiB)\n");return 2;}
    if(stat(argv[1],&st)<0){perror("stat");return 1;}
    if(!S_ISREG(st.st_mode)||st.st_size<0||st.st_size>32768L){fprintf(stderr,"Not a bounded regular file\n");return 1;}
    x=open(argv[1],O_RDONLY);y=open(argv[1],O_RDONLY);
    if(x<0||y<0){perror("open read");return 1;}
    while((n=read(x,a,sizeof(a)))>0){
        m=0;
        while(m<n){z=read(y,b+m,n-m>73?73:n-m);if(z<=0){perror("short second read");return 1;}m+=z;}
        for(i=0;i<(unsigned)n;i++){if(a[i]!=b[i]){fprintf(stderr,"Read mismatch\n");return 1;}hash=(hash^a[i])*16777619UL;}
        total+=n;
    }
    if(n<0||total!=(unsigned)st.st_size||read(y,b,1)!=0){fprintf(stderr,"Read/EOF mismatch\n");return 1;}
    if(total){
        if(lseek(x,0L,SEEK_SET)!=0L||read(x,a,1)!=1||lseek(y,0L,SEEK_SET)!=0L||read(y,b,1)!=1||a[0]!=b[0]){fprintf(stderr,"Seek mismatch\n");return 1;}
        if(lseek(x,st.st_size,SEEK_SET)!=st.st_size||read(x,a,1)!=0){fprintf(stderr,"Seek EOF mismatch\n");return 1;}
    }
    close(x);close(y);
    errno=0;z=open(argv[1],O_WRONLY);
    if(z>=0){close(z);fprintf(stderr,"Write-only open unexpectedly accepted (no write sent)\n");return 1;}
    if(errno!=EROFS&&errno!=EACCES){perror("Unexpected write-open error");return 1;}
    printf("PASS %s: %u bytes, FNV1a=%08lx\n",argv[1],total,hash);
    puts("257/73-byte reads agree; seek/EOF pass; write-only open refused.");
    return 0;
}
