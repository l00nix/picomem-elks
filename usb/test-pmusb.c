/* Exercise the actual transaction against fake ports/shared RAM only. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PMUSB_HOST_TEST 1
static unsigned char memory[65536], baseline[65536], counter;
#define MEM memory
static unsigned iflag=1, sent, writes, calls, mode, seen_polls;
#define LOCK(f) do{(f)=iflag;iflag=0;}while(0)
#define UNLOCK(f) do{assert(!iflag);iflag=(f);}while(0)
static unsigned char pin(unsigned port) {
    assert(!iflag);
    if(port==0x2a3)return mode==8?0xff:counter++;
    assert(port==0x2a0);
    if(!sent)return mode==6?1:0;
    if(mode==2)return 1; /* timeout */
    if(mode==3)return 3; /* firmware command error */
    if(mode==4)return 0; /* ignored command */
    if(mode==9&&seen_polls++<3)return 1;
    memory[0x4fc4+52]=mode==1?60:mode==5?255:84;
    memory[0x4fc4+53]=0;
    memory[0x4fc4+58]=mode==1?18:mode==10?5:0;
    memory[0x4fc4+59]=0;
    if(mode==11)memory[0x4fc4+57]^=1;
    memory[0x5000]=32;
    memcpy(memory+0x5001,"TEST    TXT",11);
    memory[0x5010]=42;
    memory[0x5011]=memory[0x5012]=memory[0x5013]=0;
    memory[0x5014]=1;memory[0x5015]=0;memory[0x5016]=7;memory[0x5017]=0;
    return 0;
}
static void pout(unsigned port,unsigned char value) {
    assert(!iflag);++writes;
    if(port==0x2a1||port==0x2a2){assert(!value);return;}
    assert(port==0x2a0&&value==0x89);
    assert(memory[0x4fc4+58]==20); /* U: only */
    unsigned char *p=memory+0x4fc4;
    if(p[59]==0x1b){assert(p[52]==74&&p[60]==0x37&&p[61]=='\\'&&!memcmp(p+62,"????????",8)&&p[70]=='.'&&!memcmp(p+71,"???",3));}
    else{assert(p[59]==0x1c&&p[52]==76&&p[64]==0x37&&!memcmp(p+65,"???????????",11));}
    ++calls;sent=1;
}
#define main pmusb_main
#include "pmusb.c"
#undef main
static unsigned cases;
static void setup(unsigned scenario) {
    mode=scenario;sent=writes=calls=seen_polls=0;counter=250;iflag=1;
    poisoned=0;seq=0;
    memset(memory,0,sizeof(memory));memset(entry,0,sizeof(entry));
    memory[0]=0x55;memory[1]=0xaa;memory[2]=0x20;
    memcpy(memory+28,"(Date 2026-06-16)",17);
    memory[0x4000]=0x12;memory[0x401b]=11;memory[0x400b]=7;memory[0x400c]=0xa8;
    memory[CFG+210]=1;memory[CFG+215]=5;memory[CFG+217]=3;
    for(unsigned i=0;i<FRAME;i++)memory[BASE+i]=0x73;
    memory[BASE]=0x12;memory[BASE+1]=0x34;memory[BASE+2]=0xaa;
    memcpy(config,memory+CFG,768);memcpy(baseline,memory,sizeof(memory));
}
static int run(int next) {
    int r=query(next);++cases;assert(iflag==1);
    assert(!memcmp(memory,baseline,BASE));
    assert(!memcmp(memory+BASE+FRAME,baseline+BASE+FRAME,sizeof(memory)-BASE-FRAME));
    return r;
}
int main(void) {
    setup(0);assert(run(0)==0&&calls==1&&polls_used==1);
    assert(!memcmp(memory,baseline,sizeof(memory))&&word(entry+20)==1&&word(entry+22)==7&&dword(entry+16)==42);
    sent=0;assert(run(1)==0&&calls==2&&!memcmp(memory,baseline,sizeof(memory)));
    setup(9);assert(run(0)==0&&polls_used==4&&!memcmp(memory,baseline,sizeof(memory)));
    setup(1);assert(run(0)==18&&!poisoned&&!memcmp(memory,baseline,sizeof(memory)));
    for(unsigned i=2;i<=5;i++){
        setup(i);assert(run(0)==(i==3?3:i==5?5:4));assert(poisoned&&calls==1);
        assert(run(0)==6&&calls==1); /* cannot retry ambiguous/error result */
        if(i==5)assert(!memcmp(memory,baseline,sizeof(memory)));
        else assert(memcmp(memory+BASE,baseline+BASE,FRAME)); /* not restored while possibly busy */
    }
    setup(6);assert(run(0)==1&&!calls&&!writes);
    setup(8);assert(run(0)==2&&!calls&&!writes);
    setup(10);assert(run(0)==7&&poisoned&&!memcmp(memory,baseline,sizeof(memory)));
    setup(11);assert(run(0)==5&&poisoned&&!memcmp(memory,baseline,sizeof(memory)));
    const unsigned bad[]={0,1,2,28,0x4000,0x401b,0x400b,0x400c,0x4005,0x4006,0x4007,0x4019,0x401a,BASE,BASE+1,BASE+2,CFG+210,CFG+215,CFG+216,CFG+217,CFG+218,CFG+234,CFG+235,CFG+42};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){
        setup(0);memory[bad[i]]^=1;memcpy(baseline,memory,sizeof(memory));
        assert(run(0)==2&&!calls&&!writes);
    }
    for(unsigned i=0;i<11;i++){
        setup(0);memory[0x4356+i]=1;memcpy(baseline,memory,sizeof(memory));
        assert(run(0)==1&&!calls&&!writes);
    }
    setup(0);assert(run(2)==2&&!calls&&!writes);
    printf("%u transaction scenarios passed; only USB root find operations issued.\n",cases);
}
