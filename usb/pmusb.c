/* SPDX-License-Identifier: GPL-2.0-or-later
 * Experimental read-only USB root listing for the validated PPC512 profile.
 * l00nix, with AI coding assistance. Protocol: PicoMEM embedded EtherDFS.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define BASE 0x4fc4
#define FRAME 84
#define CFG 0x4052
#define POLLS 262144UL
#ifndef PMUSB_HOST_TEST
#ifndef __ia16__
#error Build for ELKS/IA16 or use the hardware-free test harness
#endif
#define MEM ((volatile unsigned char __far *)0xd0000000UL)
static unsigned char pin(unsigned port) {
    unsigned char x;
    __asm__ volatile("inb %%dx,%%al":"=Ral"(x):"d"(port));return x;
}
static void pout(unsigned port,unsigned char x) {
    __asm__ volatile("outb %%al,%%dx"::"Ral"(x),"d"(port):"memory");
}
#define LOCK(f) __asm__ volatile("pushf; pop %0; cli":"=r"(f)::"memory","cc")
#define UNLOCK(f) __asm__ volatile("push %0; popf"::"r"(f):"memory","cc")
#endif
static unsigned char config[768], saved[FRAME], frame[FRAME], entry[24];
static unsigned char seq;
static int poisoned;
static unsigned long polls_used;
static unsigned word(const unsigned char *p) {return p[0]+((unsigned)p[1]<<8);}
static unsigned long dword(const unsigned char *p) {return word(p)+((unsigned long)word(p+2)<<16);}
static void putword(unsigned char *p,unsigned x) {p[0]=x;p[1]=x>>8;}

/* Caller holds CLI. Only accept the previously tested ROM/board/config.
 * No other PicoMEM source may own the command path or shared disk buffer. */
static int profile(void) {
    unsigned i;unsigned char p,n;
    static const char date[]="(Date 2026-06-16)";
    if(MEM[0]!=0x55||MEM[1]!=0xaa||MEM[2]!=0x20)return 2;
    for(i=0;i<sizeof(date)-1;i++)if(MEM[28+i]!=(unsigned char)date[i])return 2;
    if(MEM[0x4000]!=0x12||MEM[0x401b]!=11||MEM[0x400b]!=7||MEM[0x400c]!=0xa8||
       MEM[0x4005]||MEM[0x4006]||MEM[0x4007]||MEM[0x4019]||MEM[0x401a])return 2;
    if(MEM[BASE]!=0x12||MEM[BASE+1]!=0x34||MEM[BASE+2]!=0xaa)return 2;
    if(MEM[CFG+210]!=1||MEM[CFG+215]!=5||MEM[CFG+216]!=0||MEM[CFG+217]!=3||
       MEM[CFG+218]||MEM[CFG+234]||MEM[CFG+235])return 2;
    for(i=0;i<11;i++)if(MEM[0x4356+i])return 1;
    p=pin(0x2a3);
    for(i=0;i<16;i++){n=pin(0x2a3);if(n!=(unsigned char)(p+1))return 2;p=n;}
    return pin(0x2a0)?1:0;
}

/* Deliberately no caller-supplied opcode, drive, path or packet. The only
 * possible operations are FINDFIRST/FINDNEXT on U:\*.* (USB, not SD).
 * Each transaction keeps interrupts disabled until its response is copied
 * and the scratch bytes restored, preventing ELKS BIOS-disk interleaving.
 * Never reset/retry on error; an ambiguous completion poisons this process. */
static int query(int next) {
    unsigned flags,i,len,ax,request_len;
    unsigned char status;
    unsigned long tries;
    int result;
    if(poisoned)return 6;
    if(next!=0&&next!=1)return 2;
    memset(frame,0,sizeof(frame));
    frame[0]=0x12;frame[1]=0x34;frame[2]=0xaa;
    frame[12]=0xed;frame[13]=0xf5;frame[56]=1;frame[57]=++seq;
    frame[58]='U'-'A';frame[59]=next?0x1c:0x1b;
    if(next){
        memcpy(frame+60,entry+20,4);frame[64]=0x37;
        memset(frame+65,'?',11);request_len=76;
    }else{
        frame[60]=0x37;frame[61]='\\';memset(frame+62,'?',8);
        frame[70]='.';memset(frame+71,'?',3);request_len=74;
    }
    putword(frame+52,request_len);
    polls_used=0;
    LOCK(flags);
    result=profile();if(result)goto done;
    for(i=0;i<768;i++)if(MEM[CFG+i]!=config[i]){result=2;goto done;}
    for(i=0;i<FRAME;i++)saved[i]=MEM[BASE+i];
    for(i=0;i<FRAME;i++)MEM[BASE+i]=frame[i];
    pout(0x2a1,0);pout(0x2a2,0);pout(0x2a0,0x89);
    result=4;
    for(tries=0;tries<POLLS;tries++){
        ++polls_used;status=pin(0x2a0);
        if(status==2||status==3){result=3;break;}
        if(status)continue;
        len=MEM[BASE+52]+((unsigned)MEM[BASE+53]<<8);
        /* Response lengths (60/84) differ from requests (74/76).
         * This detects immediate completion without accepting ignored I/O. */
        if(len==request_len)continue;
        for(i=0;i<FRAME;i++)frame[i]=MEM[BASE+i];
        for(i=0;i<FRAME;i++)MEM[BASE+i]=saved[i];
        ax=word(frame+58);
        if(frame[0]!=0x12||frame[1]!=0x34||frame[2]!=0xaa||frame[57]!=seq||
           (len!=60&&len!=84)||ax>255){result=5;break;}
        if(ax){result=ax==0x12&&len==60?18:7;break;}
        if(len!=84){result=5;break;}
        memcpy(entry,frame+60,24);result=0;break;
    }
    if(result!=0&&result!=18)poisoned=1;
done:
    UNLOCK(flags);return result;
}

static void print_entry(void) {
    char name[13];unsigned i,n=0,end=8;
    while(end&&entry[end]==' ')--end;
    for(i=1;i<=end;i++)name[n++]=entry[i]>=32&&entry[i]<127?entry[i]:'?';
    end=3;while(end&&entry[8+end]==' ')--end;
    if(end){name[n++]='.';for(i=0;i<end;i++)name[n++]=entry[9+i]>=32&&entry[9+i]<127?entry[9+i]:'?';}
    name[n]=0;
    printf("%-12s %10lu %s  attr=%02X\n",name,dword(entry+16),entry[0]&16?"DIR ":"FILE",entry[0]);
}
int main(int argc,char **argv) {
    unsigned i,flags;int r,list;
    if(argc!=2||(strcmp(argv[1],"--check")&&strcmp(argv[1],"--list"))){
        fprintf(stderr,"Usage: pmusb --check | --list\n--list: read-only USB root, up to 32 entries.\n");return 2;
    }
    if(getuid()!=0){fprintf(stderr,"pmusb: root required\n");return 1;}
    list=!strcmp(argv[1],"--list");
    LOCK(flags);r=profile();
    if(!r)for(i=0;i<768;i++)config[i]=MEM[CFG+i];
    UNLOCK(flags);
    if(r){fprintf(stderr,"pmusb: profile/busy check failed (%d); no command sent\n",r);return 1;}
    puts("PMUSB experimental - PicoMEM USB files on ELKS");
    puts("PPC512 profile and DFS signature accepted.");
    if(!list){puts("Passive check only; no file-service command sent.");return 0;}
    puts("USB root U:\\ - read-only, 8.3 names, maximum 32 entries");fflush(stdout);
    for(i=0;i<32;i++){
        r=query(i!=0);
        if(r==18){
            if(!i){puts("No entries returned; firmware also uses this result for directory errors.");return 1;}
            printf("End of listing (%u entries).\n",i);return 0;
        }
        if(r){
            fprintf(stderr,"pmusb: stopped (%d), polls=%lu; no retry or reset\n",r,polls_used);
            if(r==3||r==4)fprintf(stderr,"Command state uncertain: reboot before further PicoMEM tests.\n");
            if(r==7)fprintf(stderr,"Firmware file-service AX=%u\n",word(frame+58));
            return 1;
        }
        print_entry();
    }
    puts("Stopped at 32 entries; directory may contain more.");return 0;
}
