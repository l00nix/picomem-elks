/* SPDX-License-Identifier: GPL-2.0-or-later
 * Included by pmcfg.c. Versioned, checked staging files; writes are opt-in.
 * Initial writable profile: PicoMEM 2 board 11, BIOS date 2026-06-16 only.
 */
#include <errno.h>
#define CONFIG_BYTES 768
#define RECORD_BYTES 1700
#define BEFORE 160
#define AFTER (BEFORE + CONFIG_BYTES)
#define CRC_OFF (AFTER + CONFIG_BYTES)
#define NIC_IRQ 215
#define FAST_BOOT 207
static unsigned char record[RECORD_BYTES], original[RECORD_BYTES];
static unsigned char livecfg[CONFIG_BYTES], verifycfg[CONFIG_BYTES];
static unsigned char identity[128];

static void putword(unsigned char *p, unsigned v)
{ p[0] = v; p[1] = v >> 8; }

static unsigned long crc32_bytes(const unsigned char *p, unsigned n)
{
    unsigned long crc = 0xffffffffUL;
    unsigned bit;
    while (n--) {
        crc ^= *p++;
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
    }
    return crc ^ 0xffffffffUL;
}

static void seal(unsigned char *p)
{
    unsigned long c = crc32_bytes(p, CRC_OFF);
    unsigned i;
    for (i = 0; i < 4; ++i) p[CRC_OFF+i] = (unsigned char)(c >> (8*i));
}

static int valid_record(const unsigned char *p)
{
    unsigned i;
    unsigned long got = 0, want = crc32_bytes(p, CRC_OFF);
    if (memcmp(p, "PMCFG02\n", 8) || word(p+12) != CONFIG_BYTES ||
        word(p+8) < 0xc000 || word(p+8) > 0xe800 ||
        (word(p+8) & 0x7f) || word(p+10) != 0x2a0 || p[14] != 11)
        return 0;
    for (i = 0; i < 4; ++i) got |= (unsigned long)p[CRC_OFF+i] << (8*i);
    if (got != want) return 0;
    for (i = 0; i < CONFIG_BYTES; ++i)
        if (i != NIC_IRQ && i != FAST_BOOT && p[BEFORE+i] != p[AFTER+i])
            return 0;
    if (p[AFTER+FAST_BOOT] > 1) return 0;
    i = p[AFTER+NIC_IRQ];
    return (i == 3 || i == 5 || i == 7) && (p[15] & (1U << i));
}

static int read_record(const char *name, unsigned char *p)
{
    int f = open(name, O_RDONLY), n;
    unsigned have = 0;
    unsigned char extra;
    if (f < 0) { perror(name); return 0; }
    while (have < RECORD_BYTES) {
        n = read(f, p+have, RECORD_BYTES-have);
        if (n <= 0) break;
        have += n;
    }
    n = read(f, &extra, 1);
    close(f);
    if (have != RECORD_BYTES || n != 0 || !valid_record(p)) {
        fprintf(stderr, "pmconfig: invalid, corrupt or unsupported staging file\n");
        return 0;
    }
    return 1;
}

static int create_record(const char *name, unsigned char *p)
{
    int f, n, ok = 1;
    unsigned done = 0;
    seal(p);
    f = open(name, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (f < 0) { perror(name); return 0; }
    while (done < RECORD_BYTES) {
        n = write(f, p+done, RECORD_BYTES-done);
        if (n <= 0) { ok = 0; break; }
        done += n;
    }
    if (close(f) < 0) ok = 0;
    if (!ok) {
        fprintf(stderr, "pmconfig: incomplete file retained; hardware untouched\n");
        return 0;
    }
    sync();
    if (!read_record(name, original) || memcmp(p, original, RECORD_BYTES)) {
        fprintf(stderr, "pmconfig: file readback mismatch; hardware untouched\n");
        return 0;
    }
    return 1;
}

static int profile_ok(void)
{
    unsigned i;
    if (snap[0] != 0x12 || snap[27] != 11 || snap[5] || snap[7]) return 0;
    for (i = 0; i+16 <= sizeof(hdr); ++i)
        if (!memcmp(hdr+i, "(Date 2026-06-16", 16)) return 1;
    return 0;
}

static int capture_config(const char *path, unsigned *seg)
{
    unsigned s, found = 0, attempt;
    unsigned long addr;
    fd = open(path, O_RDONLY);
    if (fd < 0) { perror(path); return 0; }
    for (s = 0xc000; s <= 0xe800; s += 0x80)
        if (rom_at(s)) { *seg = s; ++found; }
    if (found != 1 || !rom_at(*seg)) goto bad;
    addr = ((unsigned long)*seg << 4) + 0x4000;
    if (!read_at(addr, snap, sizeof(snap)) || !profile_ok()) goto bad;
    memcpy(identity, hdr, sizeof(identity));
    for (attempt = 0; attempt < 3; ++attempt) {
        if (!read_at(addr+CFG, livecfg, CONFIG_BYTES) ||
            !read_at(addr+CFG, verifycfg, CONFIG_BYTES)) goto bad;
        if (!memcmp(livecfg, verifycfg, CONFIG_BYTES)) break;
    }
    close(fd);
    if (attempt == 3) {
        fprintf(stderr, "pmconfig: configuration is changing; try again\n");
        return 0;
    }
    return 1;
bad:
    close(fd);
    fprintf(stderr, "pmconfig: editor requires one PicoMEM 2, BIOS 2026-06-16, healthy SD/config\n");
    return 0;
}

static void make_record(unsigned seg)
{
    memset(record, 0, sizeof(record));
    memcpy(record, "PMCFG02\n", 8);
    putword(record+8, seg);
    putword(record+10, 0x2a0);
    putword(record+12, CONFIG_BYTES);
    record[14] = snap[27]; record[15] = snap[12];
    memcpy(record+32, identity, 128);
    memcpy(record+BEFORE, livecfg, CONFIG_BYTES);
    memcpy(record+AFTER, livecfg, CONFIG_BYTES);
}

static void show_record(const unsigned char *p)
{
    printf("PicoMEM " PMCFG_VERSION " - STAGED FOR NEXT BOOT\n");
    printf("ROM %04X  management port %03X  hardware IRQ mask %02X\n",
           word(p+8), word(p+10), p[15]);
    printf("Setting          Before    Proposed\n");
    printf("NIC IRQ          %-9u %u\n", p[BEFORE+NIC_IRQ], p[AFTER+NIC_IRQ]);
    printf("Fast boot        %-9s %s\n", yesno(p[BEFORE+FAST_BOOT]),
           yesno(p[AFTER+FAST_BOOT]));
    printf("All other configuration bytes are preserved.\n");
    printf("Stage/preview change local files only. Save is a separate command.\n");
    printf("NIC changes also need matching ELKS /bootopts before reboot.\n");
}

/* Return: 0=acknowledged, 1=busy, 2=identity/stale, 3=command error,
 * 4=timeout/unknown. Error details are captured before any file/console I/O.
 * No syscall or BIOS call may occur inside this transaction.
 */
#if defined(__ia16__) || defined(PMCFG_HW_TEST)
#ifndef PMCFG_HW_TEST
static unsigned char port_in(unsigned port)
{
    unsigned char v;
    __asm__ volatile("inb %%dx,%%al" : "=Ral"(v) : "d"(port));
    return v;
}
static void port_out(unsigned port, unsigned char v)
{ __asm__ volatile("outb %%al,%%dx" : : "Ral"(v), "d"(port) : "memory"); }
#endif

static int commit_config(unsigned seg, unsigned port, unsigned char *detail)
{
#ifdef PMCFG_HW_TEST
    volatile unsigned char *mem = test_memory;
    (void)seg;
#else
    volatile unsigned char __far *mem =
        (volatile unsigned char __far *)(((unsigned long)seg << 16) | 0x4000UL);
    unsigned flags;
#endif
    unsigned i, outer;
    unsigned short inner;
    unsigned char prev, next, status = 0xff;
    int result = 2, started = 0;
#ifndef PMCFG_HW_TEST
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory", "cc");
#endif
    /* Read-only incrementing test port; test before issuing any command. */
    prev = port_in(port+3);
    for (i = 0; i < 16; ++i) {
        next = port_in(port+3);
        if (next != (unsigned char)(prev+1)) goto finished;
        prev = next;
    }
    status = port_in(port);
    if (status != 0) { result = 1; goto finished; }
    if (mem[0] != 0x12 || mem[27] != 11 || mem[12] != record[15] ||
        mem[5] != 0 || mem[7] != 0 || mem[25] || mem[26]) goto finished;
    for (i = 0; i < CONFIG_BYTES; ++i)
        if (mem[CFG+i] != record[BEFORE+i]) goto finished;
    mem[CFG+NIC_IRQ] = record[AFTER+NIC_IRQ];
    mem[CFG+FAST_BOOT] = record[AFTER+FAST_BOOT];
    if (mem[CFG+NIC_IRQ] != record[AFTER+NIC_IRQ] ||
        mem[CFG+FAST_BOOT] != record[AFTER+FAST_BOOT]) goto rollback;
    port_out(port+1, 0); port_out(port+2, 0);
    port_out(port, 0x08); /* CMD_SaveCfg, never CMD_DEV_Init */
    /* Bounded CPU-loop timeout; IRQs off prevents BIOS-disk command races.
     * Time depends on CPU speed. No allocation, printing or filesystem I/O.
     */
    for (outer = 0; outer < 16; ++outer) {
        inner = 0;
        do {
            status = port_in(port);
            if (status == 1) started = 1;
            if (status == 0 && started) { result = 0; goto finished; }
            if (status == 2 || status == 3) {
                *detail = port_in(port+1);
                result = 3; goto rollback;
            }
        } while (--inner);
    }
    result = 4; /* Do not reset a possibly active writer or restore its buffer. */
    goto finished;
rollback:
    mem[CFG+NIC_IRQ] = record[BEFORE+NIC_IRQ];
    mem[CFG+FAST_BOOT] = record[BEFORE+FAST_BOOT];
    /* A completed error must be cleared so subsequent BIOS disk I/O can run. */
    if (status == 2 || status == 3) {
        port_out(port, 0);
        inner = 0;
        do { if (port_in(port) == 0) break; } while (--inner);
    }
finished:
#ifndef PMCFG_HW_TEST
    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
#endif
    return result;
}
#endif

static void editor_usage(void)
{
    printf("Editor (PicoMEM 2 BIOS 2026-06-16):\n");
    printf("  pmconfig --stage FILE nic-irq=3|5|7 [fast-boot=on|off]\n");
    printf("  pmconfig --stage FILE fast-boot=on|off\n");
    printf("  pmconfig --show FILE\n");
    printf("  pmconfig --save FILE --backup NEWFILE\n");
    printf("  pmconfig --restore BACKUP --stage NEWFILE\n");
    printf("Stage files are exclusive, CRC-checked snapshots; save needs reboot.\n");
}

static int editor_main(int argc, char **argv)
{
    const char *path = "/dev/kmem", *dest = NULL;
    unsigned seg = 0, i, changes = 0;
    int restoring = 0, result;
    unsigned char detail = 0;
    if (argc < 3) { editor_usage(); return 2; }
    if (!strcmp(argv[1], "--show")) {
        if (argc != 3) return 2;
        if (!read_record(argv[2], record)) return 1;
        show_record(record); return 0;
    }
    /* --image is only available for file-only staging/restore tests. */
    if (argc >= 5 && !strcmp(argv[argc-2], "--image")) {
        if (!strcmp(argv[1], "--save")) {
            fprintf(stderr, "pmconfig: saving from a fixture is prohibited\n"); return 2;
        }
        path = argv[argc-1]; argc -= 2;
    }
    if (strcmp(argv[1], "--stage") && strcmp(argv[1], "--save") &&
        strcmp(argv[1], "--restore")) { editor_usage(); return 2; }
    if (!capture_config(path, &seg)) return 1;
    if (!strcmp(argv[1], "--save")) {
        if (argc != 5 || strcmp(argv[3], "--backup")) {
            editor_usage(); return 2;
        }
        if (!read_record(argv[2], record)) return 1;
        if (word(record+8) != seg || memcmp(record+32, identity, 128) ||
            record[14] != snap[27] || record[15] != snap[12] ||
            memcmp(record+BEFORE, livecfg, CONFIG_BYTES)) {
            fprintf(stderr, "pmconfig: stale stage or different board; stage again\n"); return 1;
        }
        show_record(record);
#ifndef __ia16__
        (void)detail; (void)result;
        fprintf(stderr, "pmconfig: hardware saving requires the ELKS i8086 build\n");
        return 1;
#else
        if (geteuid() != 0) { fprintf(stderr, "pmconfig: save requires root\n"); return 1; }
        /* A rollback record stores the complete original configuration twice. */
        memcpy(record+AFTER, record+BEFORE, CONFIG_BYTES);
        if (!create_record(argv[4], record)) return 1;
        if (!read_record(argv[2], record)) return 1;
        printf("Rollback file verified: %s\n", argv[4]);
        fflush(stdout);
        sync();
        result = commit_config(seg, word(record+10), &detail);
        if (result) {
            fprintf(stderr, "pmconfig: save failed/uncertain (code %d, firmware %u)\n",
                    result, detail);
            fprintf(stderr, "Do not reboot automatically. Backup: %s\n", argv[4]);
            return 1;
        }
        printf("Firmware acknowledged SAVE. Shared settings are pending reboot.\n");
        printf("Reboot to apply. SD-file readback is not implemented.\n");
        return 0;
#endif
    }
    if (!strcmp(argv[1], "--restore")) {
        if (argc != 5 || strcmp(argv[3], "--stage")) { editor_usage(); return 2; }
        if (!read_record(argv[2], original)) return 1;
        if (memcmp(original+32, identity, 128) || original[14] != snap[27]) {
            fprintf(stderr, "pmconfig: backup belongs to another firmware profile\n"); return 1;
        }
        dest = argv[4]; restoring = 1;
    } else {
        dest = argv[2];
        if (argc < 4) { editor_usage(); return 2; }
    }
    make_record(seg);
    if (restoring) {
        record[AFTER+NIC_IRQ] = original[BEFORE+NIC_IRQ];
        record[AFTER+FAST_BOOT] = original[BEFORE+FAST_BOOT];
    } else for (i = 3; i < (unsigned)argc; ++i) {
        if (!strncmp(argv[i], "nic-irq=", 8) && strlen(argv[i]) == 9 &&
            (argv[i][8] == '3' || argv[i][8] == '5' || argv[i][8] == '7')) {
            if (changes & 1) return 2;
            record[AFTER+NIC_IRQ] = argv[i][8]-'0'; changes |= 1;
        } else if (!strcmp(argv[i], "fast-boot=on") || !strcmp(argv[i], "fast-boot=off")) {
            if (changes & 2) return 2;
            record[AFTER+FAST_BOOT] = !strcmp(argv[i], "fast-boot=on"); changes |= 2;
        } else {
            fprintf(stderr, "pmconfig: unsupported setting: %s\n", argv[i]); return 2;
        }
    }
    seal(record);
    if (!valid_record(record)) {
        fprintf(stderr, "pmconfig: IRQ unavailable or invalid setting; nothing staged\n"); return 2;
    }
    show_record(record);
    if (!create_record(dest, record)) return 1;
    printf("Created %s. PicoMEM is unchanged.\n", dest);
    return 0;
}
