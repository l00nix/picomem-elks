/* Compile-time fake hardware exercises the actual guarded transaction code.
 * This backend is never included in the installed binary. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define PMCFG_HW_TEST 1
static unsigned char test_memory[850], saved[768];
static unsigned char test_counter;
static int scenario, sent, polls, reset_sent, save_count;
static unsigned char port_in(unsigned port)
{
    if (port == 0x2a3) return scenario == 1 ? 0xff : test_counter++;
    if (port == 0x2a1) return 7;
    if (port != 0x2a0) return 0;
    if (scenario == 2 && !sent) return 1;
    if (!sent) return 0;
    if (scenario == 3) return ++polls == 1 ? 1 : 2;
    if (scenario == 4) return 1;
    if (scenario == 5) return 0;
    if (polls++ == 0) return 1;
    memcpy(saved,test_memory+82,768);
    return 0;
}
static void port_out(unsigned port, unsigned char value)
{
    if (port != 0x2a0) return;
    if (value == 8) { sent=1; ++save_count; }
    else if (value == 0) { sent=0; ++reset_sent; }
    else assert(!"Unexpected firmware command");
}
#define main utility_main
#include "pmcfg.c"
#undef main

static void setup(int s)
{
    unsigned i;
    scenario=s; sent=polls=reset_sent=save_count=0;test_counter=250;
    memset(test_memory,0,sizeof(test_memory));
    memset(record,0,sizeof(record));
    memset(saved,0,sizeof(saved));
    test_memory[0]=0x12;test_memory[27]=11;test_memory[12]=0xa8;
    record[15]=0xa8;
    for(i=0;i<768;i++) record[BEFORE+i]=record[AFTER+i]=(unsigned char)i;
    record[BEFORE+NIC_IRQ]=3;record[AFTER+NIC_IRQ]=5;
    record[BEFORE+FAST_BOOT]=0;record[AFTER+FAST_BOOT]=1;
    memcpy(test_memory+82,record+BEFORE,768);
}

int main(void)
{
    unsigned char detail=0;
    setup(0);
    assert(commit_config(0xd000,0x2a0,&detail)==0);
    assert(save_count==1 && reset_sent==0);
    assert(!memcmp(saved,record+AFTER,768));
    assert(!memcmp(test_memory+82,record+AFTER,768));
    setup(1); assert(commit_config(0xd000,0x2a0,&detail)==2);
    assert(!save_count && !memcmp(test_memory+82,record+BEFORE,768));
    setup(2); assert(commit_config(0xd000,0x2a0,&detail)==1);
    assert(!save_count && !memcmp(test_memory+82,record+BEFORE,768));
    setup(0); test_memory[82+120]^=1;
    assert(commit_config(0xd000,0x2a0,&detail)==2); assert(!save_count);
    setup(0); test_memory[26]=1;
    assert(commit_config(0xd000,0x2a0,&detail)==2); assert(!save_count);
    setup(0); test_memory[12]=0x88;
    assert(commit_config(0xd000,0x2a0,&detail)==2); assert(!save_count);
    setup(3); assert(commit_config(0xd000,0x2a0,&detail)==3);
    assert(detail==7 && reset_sent==1 && save_count==1);
    assert(!memcmp(test_memory+82,record+BEFORE,768));
    setup(4); assert(commit_config(0xd000,0x2a0,&detail)==4);
    assert(reset_sent==0 && save_count==1);
    assert(!memcmp(test_memory+82,record+AFTER,768));
    setup(5); assert(commit_config(0xd000,0x2a0,&detail)==4);
    assert(reset_sent==0 && save_count==1);
    puts("9 transaction scenarios passed: success, absent port, busy, stale,");
    puts("PC-command busy, changed IRQ mask, firmware error, timeout, ignored command.");
    return 0;
}
