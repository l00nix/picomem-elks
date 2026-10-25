/* Fake ports exercise the native query transaction without hardware. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define PMQUERY_HW_TEST 1
static unsigned char test_memory[850], counter, arg, high;
static unsigned polls, queries, resets;
static int scenario, sent;
static unsigned char port_in(unsigned port)
{
    if (port == 0x2a3) return scenario == 2 ? 0xff : counter++;
    if (port == 0x2a1) return scenario == 7 ? arg + 1 : arg;
    if (port == 0x2a2) return high;
    assert(port == 0x2a0);
    if (!sent) return scenario == 3 || (scenario == 8 && resets) ? 1 : 0;
    if (scenario == 4) return 0; /* ignored command, sentinel unchanged */
    if (scenario == 5) return 1; /* stuck busy */
    if (scenario == 6 || scenario == 8) return 3;
    if (scenario != 1 && polls++ == 0) return 1;
    high = 8; return 0;
}
static void port_out(unsigned port, unsigned char value)
{
    if (port == 0x2a1) { arg = value; return; }
    if (port == 0x2a2) { high = value; return; }
    assert(port == 0x2a0);
    if (value == 5) { ++queries; sent = 1; }
    else { assert(value == 0); ++resets; sent = 0; }
}
#define main utility_main
#include "pmcfg.c"
#undef main

static void setup(int mode)
{
    scenario = mode; sent = 0; polls = queries = resets = 0; counter = 250;
    memset(test_memory, 0, sizeof(test_memory));
    memset(snap, 0, sizeof(snap)); memset(livecfg, 0x35, sizeof(livecfg));
    test_memory[0] = 0x12; test_memory[27] = 11;
    test_memory[12] = snap[12] = 0xa8;
    memcpy(test_memory + CFG, livecfg, CONFIG_BYTES);
}

int main(void)
{
    unsigned i, cases = 0;
    unsigned char answer = 0;
    const unsigned invalid[] = {0,27,12,5,7,25,26,CFG+215};
    for (i = 0; i < 2; ++i) {
        setup(i); assert(memory_query(0xd000,52,&answer) == 0 && answer == 8);
        assert(queries == 1 && !resets); ++cases;
    }
    setup(2); assert(memory_query(0xd000,52,&answer) == 2 && !queries); ++cases;
    setup(3); assert(memory_query(0xd000,52,&answer) == 1 && !queries); ++cases;
    for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        setup(0); test_memory[invalid[i]] ^= 1;
        assert(memory_query(0xd000,52,&answer) == 2 && !queries); ++cases;
    }
    setup(0); assert(memory_query(0xd000,64,&answer) == 2 && !queries); ++cases;
    for (i = 4; i <= 5; ++i) {
        setup(i); assert(memory_query(0xd000,52,&answer) == 4);
        assert(queries == 1 && !resets); ++cases;
    }
    setup(6); assert(memory_query(0xd000,52,&answer) == 3);
    assert(queries == 1 && resets == 1); ++cases;
    setup(7); assert(memory_query(0xd000,52,&answer) == 5); ++cases;
    setup(8); assert(memory_query(0xd000,52,&answer) == 4 && resets == 1); ++cases;
    assert(!memcmp(test_memory + CFG, livecfg, CONFIG_BYTES));
    printf("%u query scenarios passed; no configuration writes.\n", cases);
    return 0;
}
