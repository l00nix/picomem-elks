/* SPDX-License-Identifier: GPL-2.0-or-later
 * Explicit USB/disk status queries. The WiFi command is never permitted.
 * Parameters begin at 886 in the supported 768-byte configuration profile.
 */
#define STATUS_PARAM 886
#define STATUS_BYTES 1024
#define STATUS_LINES 8
#define STATUS_LINE_BYTES 127
static unsigned status_polls, status_used;

#if defined(__ia16__) || defined(PMSTATUS_HW_TEST)
static int status_query(unsigned seg, unsigned command)
{
#ifdef PMSTATUS_HW_TEST
    volatile unsigned char *mem = test_memory;
    (void)seg;
#else
    volatile unsigned char __far *mem =
        (volatile unsigned char __far *)(((unsigned long)seg << 16) | 0x4000UL);
    unsigned flags;
#endif
    unsigned i, line, pos, length;
    unsigned char prev, next, status, count, c;
    int result = 2;
    status_polls = status_used = 0;
    if (command != 0x61 && command != 0x62) return 2;
#ifndef PMSTATUS_HW_TEST
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
    /* Clear only our bounded command scratch range, after ownership checks.
     * FF detects an ignored command and prevents stale strings being emitted.
     * In particular, never reuse bytes left by an earlier WiFi response.
     */
    for (i = 0; i < STATUS_BYTES; ++i) mem[STATUS_PARAM+i] = 0xff;
    port_out(0x2a1, 0); port_out(0x2a2, 0);
    port_out(0x2a0, (unsigned char)command);
    result = 4;
    for (i = 0; i < 32768U; ++i) {
        status_polls = i + 1;
        status = port_in(0x2a0);
        if (status == 0) {
            count = mem[STATUS_PARAM];
            if (count == 0xff) continue;
            result = 5;
            if (count > STATUS_LINES || (command == 0x62 && count > 6)) goto done;
            record[0] = count;
            pos = 1;
            for (line = 0; line < count; ++line) {
                length = 0;
                do {
                    if (pos >= STATUS_BYTES || length > STATUS_LINE_BYTES) goto done;
                    c = mem[STATUS_PARAM+pos];
                    if (c == 0xff) goto done;
                    /* Firmware FE is a display bullet. Sanitize all controls. */
                    record[pos++] = c == 0xfe ? ' ' :
                        c && (c < 32 || c > 126) ? '?' : c;
                    if (c) ++length;
                } while (c);
            }
            if (mem[STATUS_PARAM] != count) goto done;
            status_used = pos;
            result = 0; goto done;
        }
        if (status == 2 || status == 3) {
            port_out(0x2a0, 0); /* Reset only our completed command error. */
            for (i = 0; i < 8192U; ++i)
                if (port_in(0x2a0) == 0) { result = 3; goto done; }
            goto done;
        }
    }
done:
#ifndef PMSTATUS_HW_TEST
    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
#endif
    return result;
}
#endif

static int report_live_status(unsigned seg, unsigned command)
{
#if defined(__ia16__) || defined(PMSTATUS_HW_TEST)
    unsigned i, pos;
    int result;
    if (getuid() != 0) { fprintf(stderr, "pmstat: live queries require root\n"); return 1; }
    printf("PMSTAT " PMCFG_VERSION " - %s status query (CMD %02X)\n",
           command == 0x61 ? "USB" : "disk", command);
    fflush(stdout);
    result = status_query(seg, command);
    if (result) {
        fprintf(stderr, "pmstat: query failed (%d); no response printed or retry\n", result);
        fprintf(stderr, "1=busy 2=identity/config 3=firmware error 4=timeout 5=bad reply\n");
        return 1;
    }
    if (!record[0]) printf("Firmware returned no status lines (disabled/unavailable).\n");
    for (i = 0, pos = 1; i < record[0]; ++i) {
        printf("%s\n", record + pos);
        pos += strlen((char *)record + pos) + 1;
    }
    printf("Copied %u bounded bytes; ready after %u status polls.\n", status_used, status_polls);
    printf("Firmware snapshot; not proof of input events or an ELKS mount.\n");
    printf("No WiFi query, mouse enable, configuration save or remapping.\n");
    return 0;
#else
    (void)seg; (void)command; (void)status_polls; (void)status_used;
    fprintf(stderr, "pmstat: live queries require the native ELKS build\n");
    return 1;
#endif
}
