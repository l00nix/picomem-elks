/* Fake command ports/shared RAM: no real hardware I/O is compiled. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PMSTATUS_HW_TEST 1
static unsigned char test_memory[2048], baseline[2048], counter;
static unsigned char response[1024];
static unsigned response_len, polls, sent, queries, resets, last_command;
static int scenario;
static unsigned char port_in(unsigned port)
{
    if (port == 0x2a3) return scenario == 2 ? 0xff : counter++;
    assert(port == 0x2a0);
    if (!sent) return scenario == 3 || (scenario == 8 && resets) ? 1 : 0;
    if (scenario == 4) return 0; /* Ignored, sentinel remains FF. */
    if (scenario == 5) return 1;
    if (scenario == 6 || scenario == 8) return 3;
    if (scenario != 1 && polls++ < 3) return 1;
    memcpy(test_memory + 886, response, response_len);
    return 0;
}
static void port_out(unsigned port, unsigned char value)
{
    if (port == 0x2a1 || port == 0x2a2) { assert(!value); return; }
    assert(port == 0x2a0);
    if (value) {
        assert(value == 0x61 || value == 0x62);
        last_command = value; ++queries; sent = 1;
        for (unsigned i = 0; i < 1024; ++i) assert(test_memory[886+i] == 0xff);
    } else { ++resets; sent = 0; }
}
#define main utility_main
#include "pmcfg.c"
#undef main
static unsigned cases;
static void setup(int mode)
{
    scenario = mode; sent = polls = queries = resets = last_command = 0; counter = 250;
    memset(test_memory, 0x73, sizeof(test_memory));
    memset(snap, 0, sizeof(snap)); memset(livecfg, 0x35, sizeof(livecfg));
    test_memory[0] = 0x12; test_memory[27] = 11;
    test_memory[12] = snap[12] = 0xa8;
    test_memory[5] = test_memory[7] = test_memory[25] = test_memory[26] = 0;
    memcpy(test_memory + CFG, livecfg, CONFIG_BYTES);
    memcpy(baseline, test_memory, sizeof(baseline));
    memset(response, 0xff, sizeof(response));
    response[0] = 1; memcpy(response + 1, "USB mouse", 10); response_len = 11;
}
static int run(unsigned command)
{
    int r = status_query(0xd000, command);
    assert(!memcmp(test_memory, baseline, STATUS_PARAM));
    assert(!memcmp(test_memory+STATUS_PARAM+STATUS_BYTES,
                   baseline+STATUS_PARAM+STATUS_BYTES,
                   sizeof(test_memory)-STATUS_PARAM-STATUS_BYTES));
    if (!queries) assert(!memcmp(test_memory, baseline, sizeof(test_memory)));
    ++cases; return r;
}
int main(void)
{
    unsigned i;
    const unsigned invalid[] = {0,27,12,5,7,25,26,CFG+215};
    for (i = 0; i < 2; ++i) {
        setup(i); assert(run(0x61) == 0 && status_used == 11 && queries == 1 && !resets);
        assert(!strcmp((char *)record+1,"USB mouse"));
        assert(status_polls == (i ? 1 : 4));
    }
    setup(0); assert(run(0x62) == 0 && last_command == 0x62);
    setup(2); assert(run(0x61) == 2 && !queries);
    setup(3); assert(run(0x61) == 1 && !queries);
    for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        setup(0); test_memory[invalid[i]] ^= 1;
        memcpy(baseline,test_memory,sizeof(baseline));
        assert(run(0x61) == 2 && !queries);
    }
    for (i = 4; i <= 5; ++i) {
        setup(i); assert(run(0x61) == 4 && status_polls == 32768U && queries == 1 && !resets);
    }
    setup(6); assert(run(0x61) == 3 && resets == 1);
    setup(8); assert(run(0x61) == 4 && resets == 1);
    setup(0); assert(run(0x60) == 2 && !queries); /* WiFi explicitly prohibited. */
    setup(0); assert(run(0x52) == 2 && !queries); /* No mouse enable. */
    setup(0); response[0]=0; response_len=1;
    assert(run(0x61)==0 && status_used==1);
    setup(0); response[0]=9;
    assert(run(0x61)==5 && !status_used);
    setup(0); response[0]=7;
    assert(run(0x62)==5 && !status_used);
    setup(0); response[0]=2; /* Missing second string: old scratch never emitted. */
    assert(run(0x61)==5 && !status_used);
    setup(0); memset(response+1,'a',128); response[129]=0; response_len=130;
    assert(run(0x61)==5 && !status_used);
    setup(0); memset(response+1,'a',127); response[128]=0; response_len=129;
    assert(run(0x61)==0 && status_used==129);
    setup(0); response[0]=8;
    for (i=0;i<8;++i) {memset(response+1+i*127,'a',126);response[127+i*127]=0;}
    response_len=1017;
    assert(run(0x61)==0 && status_used==1017);
    setup(0); response[0]=8;
    for (i=0;i<8;++i) {memset(response+1+i*128,'a',127);if(i<7)response[128+i*128]=0;}
    response_len=1024;
    assert(run(0x61)==5 && !status_used); /* Last terminator outside the bound. */
    setup(0); response[1]=0xfe;response[2]=27;response[3]='\n';response[4]=0x80;
    assert(run(0x61)==0 && !memcmp(record+1," ???",4));
    /* The transport diagnostic adds three cached lines to the existing wire
     * format. Exercise an eight-line inventory plus the exact text fields. */
    {
        const char *lines[] = {"USB: 4 devices", "1: fault", "2: hub", "3: mouse", "4: keyboard",
            "USB fault op=1a tag=4294967295 why=1 stage=2 event=2",
            "USB xfer ep=81/81 got=0 want=64 data=0",
            "USB CSW checks=00 status=255 residue=4294967295"};
        setup(0); response[0]=8; response_len=1;
        for (i=0;i<8;++i) {
            unsigned length=(unsigned)strlen(lines[i])+1;
            memcpy(response+response_len,lines[i],length);response_len+=length;
        }
        assert(run(0x61)==0 && status_used==response_len && queries==1 && !resets);
        assert(!memcmp(record,response,response_len));
    }
    printf("%u status query scenarios passed; only bounded command scratch changed.\n",cases);
    return 0;
}
