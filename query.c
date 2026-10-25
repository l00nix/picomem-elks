/* SPDX-License-Identifier: GPL-2.0-or-later
 * Active memory-type queries only. No memory probes, remapping, or WiFi calls.
 */
#if defined(__ia16__) || defined(PMQUERY_HW_TEST)
static int memory_query(unsigned seg, unsigned block, unsigned char *answer)
{
#ifdef PMQUERY_HW_TEST
    volatile unsigned char *mem = test_memory;
    (void)seg;
#else
    volatile unsigned char __far *mem =
        (volatile unsigned char __far *)(((unsigned long)seg << 16) | 0x4000UL);
    unsigned flags;
#endif
    unsigned i;
    unsigned char prev, next, status, high;
    int result = 2;
    if (block >= 64) return 2;
#ifndef PMQUERY_HW_TEST
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory", "cc");
#endif
    prev = port_in(0x2a3);
    for (i = 0; i < 16; ++i) {
        next = port_in(0x2a3);
        if (next != (unsigned char)(prev + 1)) goto done;
        prev = next;
    }
    if (port_in(0x2a0) != 0) { result = 1; goto done; }
    if (mem[0] != 0x12 || mem[27] != 11 || mem[12] != snap[12] ||
        mem[5] || mem[7] || mem[25] || mem[26]) goto done;
    for (i = 0; i < CONFIG_BYTES; ++i)
        if (mem[CFG+i] != livecfg[i]) goto done;
    port_out(0x2a1, (unsigned char)block);
    port_out(0x2a2, 0xff); /* A completed CMD 05 replaces this sentinel. */
    port_out(0x2a0, 5);
    result = 4;
    for (i = 0; i < 8192; ++i) {
        status = port_in(0x2a0);
        if (status == 0) {
            high = port_in(0x2a2);
            if (high == 0xff) continue;
            if (port_in(0x2a1) != block) { result = 5; goto done; }
            *answer = high; result = 0; goto done;
        }
        if (status == 2 || status == 3) {
            /* Clear only an error produced by our completed command. */
            port_out(0x2a0, 0);
            for (i = 0; i < 8192; ++i)
                if (port_in(0x2a0) == 0) { result = 3; goto done; }
            goto done;
        }
    }
done:
#ifndef PMQUERY_HW_TEST
    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
#endif
    return result;
}
#endif

static int report_memory(unsigned seg, int probe)
{
#if defined(__ia16__) || defined(PMQUERY_HW_TEST)
    unsigned i, bios = seg >> 10, diff = 0, emulated = 0;
    unsigned char active[64], expected;
    char map[65];
    int result;
    if (getuid() != 0) { fprintf(stderr, "pmmem: root required\n"); return 1; }
    printf("PMMEM " PMCFG_VERSION " - querying active memory types (CMD 05)\n");
    fflush(stdout);
    result = memory_query(seg, bios, active + bios);
    if (result) goto failed;
    if (active[bios] != 8 && active[bios] != 12) {
        fprintf(stderr, "pmmem: BIOS block returned %02X; stopping\n", active[bios]);
        return 1;
    }
    if (probe) {
        printf("ROM %04X:0000 block %u: BIOS type %02X verified\n", seg, bios, active[bios]);
        printf("One query only. No memory writes or configuration changes.\n");
        return 0;
    }
    for (i = 0; i < 64; ++i) {
        if (i != bios) {
            result = memory_query(seg, i, active + i);
            if (result) goto failed;
        }
        if (active[i]) ++emulated;
        expected = livecfg[138+i];
        if (expected >= 32) expected = 0; /* PC-owned, not card emulation. */
        if (active[i] != expected) ++diff;
        map[i] = memchar(active[i]);
    }
    map[64] = 0;
    printf("Each character = 16 KiB; physical 00000h to FFFFFh\n");
    printf("Active card map:\n%s\n", map);
    for (i = 0; i < 64; ++i) map[i] = memchar(livecfg[138+i]);
    printf("Configured map (also describes PC-owned RAM/video/ROM):\n%s\n", map);
    printf("r=card RAM p=PSRAM e=EMS B=BIOS d=buffer R=ROM .=not emulated\n");
    printf("s/v/o describe the PC; active '.' is expected for these blocks.\n");
    printf("Emulated blocks: %u  Configuration differences: %u\n", emulated, diff);
    for (i = 0; i < 64; ++i) {
        expected = livecfg[138+i]; if (expected >= 32) expected = 0;
        if (active[i] != expected)
            printf("  %04X:0000 active %02X  configured %02X\n", i << 10, active[i], livecfg[138+i]);
    }
    printf("Sampled across 64 queries. No RAM write probes or remapping.\n");
    return 0;
failed:
    fprintf(stderr, "pmmem: query failed (%d); stopped without retry\n", result);
    fprintf(stderr, "1=busy 2=identity/config 3=firmware error 4=timeout 5=bad reply\n");
    return 1;
#else
    (void)seg; (void)probe;
    fprintf(stderr, "pmmem: live queries require the native ELKS build\n");
    return 1;
#endif
}
