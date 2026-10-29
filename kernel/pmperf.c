/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
static unsigned char b[52];
static unsigned word(unsigned n){return b[n]+((unsigned)b[n+1]<<8);}
static unsigned long number(unsigned n){return word(n)+((unsigned long)word(n+2)<<16);}
static int query(int fd,unsigned op,unsigned cmd)
{
 memset(b,0,sizeof(b));memcpy(b,"PMDG",4);b[4]=1;b[5]=op;b[6]=cmd;
 if(ioctl(fd,0x5044,b)<0){perror("pmperf ioctl");return -1;}
 if(memcmp(b,"PMDG",4)||b[4]!=1||b[5]!=op||b[6]!=cmd||word(26)!=4096||word(42)!=1){fprintf(stderr,"pmperf: unexpected diagnostic reply\n");return -1;}
 return 0;
}
int main(int argc,char **argv)
{
 int fd,reset=0,bad=0;unsigned j;const char *mount;
 static const unsigned commands[]={255,0x70,0x71,0x72,0x1b,0x1c,0x16,0x08,0x09,0x06,0x0f,0x03,0x01,0x13,0x17,0x2e};
 if(argc==3&&!strcmp(argv[1],"--reset")){reset=1;mount=argv[2];}
 else if(argc==2)mount=argv[1];else{fprintf(stderr,"Usage: pmperf [--reset] /mnt/usb\n");return 2;}
 if(getuid()){fprintf(stderr,"pmperf: root required\n");return 1;}
 fd=open(mount,O_RDONLY);if(fd<0){perror("open USB mount");return 1;}
 if(query(fd,reset,255)){close(fd);return 1;}
 printf("PMDIAG v1 %s; firmware microseconds; stack includes instrumentation\n",reset?"RESET":"REPORT");
 printf("FW_STACK used=%u reserved=%u flags=%u\n",word(24),word(26),b[7]);
 printf("K_STACK used=%u reserved=%u pid=%u syscall=%u invalid=%u samples=%lu\n",word(32),word(34),word(36),word(38),word(40),number(44));
 bad=b[7]||word(40)||word(24)>=word(26)||word(32)>=word(34);
 if(!reset){
  puts("CMD COUNT MIN_US MAX_US TOTAL_US FLAGS");
  for(j=0;j<sizeof(commands)/sizeof(commands[0]);j++){
   if(query(fd,0,commands[j])){close(fd);return 1;}
   printf("%02x %lu %lu %lu %lu %u\n",commands[j],number(8),number(12),number(16),number(20),b[7]);
   if(b[7])bad=1;
  }
 }
 close(fd);return bad?1:0;
}
