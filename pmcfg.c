/* SPDX-License-Identifier: GPL-2.0-or-later
 * pmconfig: PicoMEM inspection and staged next-boot settings for ELKS/i8086.
 * Wire offsets documented by FreddyVRetro/ISA-PicoMEM (pm_defines.h)
 * and jdredd87/CH375USBTools (PicoMEM/src/pmcfg.pas).
 * Inspection is read only. Explicit --save uses the guarded editor backend.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define CFG 82
#define SNAPLEN (CFG + 256)
#define PMCFG_VERSION "0.5-dev"
static unsigned char snap[SNAPLEN], check[SNAPLEN], hdr[128];
static int fd;

static unsigned word(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static int read_at(unsigned long address, unsigned char *buf, unsigned len)
{
    int n;
    if (lseek(fd, (long)address, SEEK_SET) != (long)address) return 0;
    while (len) {
        n = read(fd, buf, len);
        if (n <= 0) return 0;
        buf += n;
        len -= n;
    }
    return 1;
}

static int rom_at(unsigned seg)
{
    if (!read_at((unsigned long)seg << 4, hdr, sizeof(hdr))) return 0;
    return hdr[0] == 0x55 && hdr[1] == 0xaa && hdr[2] == 32 &&
           memcmp(hdr + 6, "PMBIOS", 6) == 0;
}

static void clean_name(char *dst, const unsigned char *src, unsigned n)
{
    unsigned i;
    for (i = 0; i < n && src[i]; ++i)
        dst[i] = src[i] >= 32 && src[i] < 127 ? src[i] : '?';
    while (i && dst[i-1] == ' ') --i;
    dst[i] = 0;
}

static const char *state(unsigned s)
{
    switch (s) {
    case 0: return "OK";
    case 0xfc: return "skipped/error";
    case 0xfd: return "failed/busy";
    case 0xfe: return "pending";
    case 0xff: return "disabled";
    default: return "unknown";
    }
}

static const char *yesno(unsigned s)
{
    return s == 0 ? "off" : s == 1 ? "on" : "other";
}

static char memchar(unsigned s)
{
    switch (s) {
    case 0: return '.'; case 1: return 'r'; case 2: return 'd';
    case 8: return 'B'; case 9: return 'R'; case 10: return 'R';
    case 12: return 'B'; case 16: return 'p'; case 17: return 'e';
    case 32: return 's'; case 33: return 'v'; case 34: return 'o';
    default: return '?';
    }
}

static void disk(unsigned i, int floppy)
{
    unsigned off = floppy ? i * 16 : 32 + i * 16;
    unsigned attr = snap[CFG + (floppy ? 96 : 98) + i];
    char name[15];
    clean_name(name, snap + CFG + off + 2, 14);
    printf("  %s%u %-14s %5u %-2s  %s (attr %02X)\n",
        floppy ? "FDD" : "HDD", i, name[0] ? name : "-",
        word(snap + CFG + off), floppy ? "KB" : "MB",
        attr & 0x80 ? "image" : "PC/off", attr);
}

static void report(unsigned seg, int fixture, int extended)
{
    unsigned i, port = word(snap + CFG + 216), irq = snap[CFG + 215];
    unsigned ems = snap[CFG + 202];
    static const unsigned emsports[] = {0, 0x268, 0x288, 0x298, 0x2a8};
    char date[11], map[65];
    strcpy(date, "unknown");
    for (i = 0; i + 16 < sizeof(hdr); ++i) {
        if (!memcmp(hdr + i, "(Date ", 6)) {
            clean_name(date, hdr + i + 6, 10);
            break;
        }
    }
    for (i = 0; i < 64; ++i) map[i] = memchar(snap[CFG + 138 + i]);
    map[64] = 0;
    printf("PicoMEM configuration " PMCFG_VERSION "  |  ELKS  |  READ ONLY%s\n",
           fixture ? " [FILE]" : "");
    printf("ROM %04X:0000  BIOS %s  Shared %04X:4000  ID %02X\n",
           seg, date, seg, snap[0]);
    printf("Layout: %s\n", extended ?
           "768-byte profile, board 11 / BIOS 2026-06-16" :
           "common 256-byte config prefix; extended fields not decoded");
    printf("Init: SD %s [%02X]  WiFi %s [%02X]  USB %s [%02X]\n",
           state(snap[5]), snap[5], state(snap[8]), snap[8],
           state(snap[6]), snap[6]);
    printf("Config init: %s [%02X]  PSRAM code: %02X  HW IRQ: %u\n",
           state(snap[7]), snap[7], snap[4], snap[11]);
    printf("NE2000: %s  port 0x%03X  IRQ %u  WiFi enable flag: %u\n",
           port ? "enabled" : "disabled", port, irq, snap[CFG + 214]);
    if (port >= 0x200 && port <= 0x3e0 && !(port & 31) &&
        irq > 0 && irq < 8)
        printf("ELKS bootopts candidate: ne0=%u,0x%03X,,0x92 (verify IRQ routing)\n",
               irq, port);
    else
        printf("ELKS bootopts: no candidate; check the reported port and IRQ\n");
    if (snap[27] == 11)
        printf("Hardware IRQ mask: 0x%02X  NIC IRQ direct: %s\n", snap[12],
               irq < 8 && (snap[12] & (1U << irq)) ? "available" : "not reported");
    else if (port && irq != snap[11])
        printf("IRQ note: NE2000 and BIOS HW IRQ differ; confirm board routing.\n");
    printf("RAM extension: SRAM %s  PSRAM %s  Max conventional %s\n",
           yesno(snap[CFG + 204]), yesno(snap[CFG + 205]),
           yesno(snap[CFG + 206]));
    if (ems > 0 && ems < 5 && snap[CFG + 203] < 2)
        printf("EMS: port 0x%03X  frame %s000h  USB %s  Joystick %s\n",
               emsports[ems], snap[CFG + 203] ? "E" : "D",
               yesno(snap[CFG + 210]), yesno(snap[CFG + 218]));
    else
        printf("EMS index: %u (0=off)  USB %s  Joystick %s\n", ems,
               yesno(snap[CFG + 210]), yesno(snap[CFG + 218]));
    printf("Configured map: each character = 16 KiB, from 00000h to FFFFFh\n");
    printf("%s\n", map);
    printf("s=PC RAM r=card RAM p=PSRAM e=EMS v=video B=BIOS o=PC ROM .=none\n");
    printf("Configured disks (names, not filesystem mount status):\n");
    for (i = 0; i < 2; ++i) disk(i, 1);
    for (i = 0; i < 4; ++i) disk(i, 0);
    printf("Boot disk index: %u  Fast boot %s  Card bootstrap %s\n",
           snap[CFG + 213], yesno(snap[CFG + 207]), yesno(snap[CFG + 208]));
    printf("Snapshot only: no firmware commands, port I/O or config changes.\n");
}

#include "editor.c"
#include "details.c"
#include "dump.c"
#include "query.c"
#include "status.c"

int main(int argc, char **argv)
{
    const char *path = "/dev/kmem", *dump_path = NULL, *name;
    unsigned seg = 0, found = 0, candidate, attempt, status_command = 0;
    int i, start = 1, status_mode = 0, memory_mode = 0;
    int verbose = 0, fixture = 0, details = 0, extended = 0, captured;
    char *end;
    unsigned long value;
    name = strrchr(argv[0], '/'); name = name ? name + 1 : argv[0];
    if (!strcmp(name, "pmdump")) {
        if (argc < 2 || argv[1][0] == '-') {
            printf("Usage: pmdump NEWFILE [-f MEMORY_IMAGE] [-s HEXSEG]\n");
            return argc == 2 && (!strcmp(argv[1], "--help") ||
                   !strcmp(argv[1], "-h")) ? 0 : 2;
        }
        dump_path = argv[1]; details = 1; start = 2;
    } else if (!strcmp(name, "pmmem")) memory_mode = 1;
    else if (!strcmp(name, "pmstat")) status_mode = 1;
    if (argc > 1 && (!strcmp(argv[1], "--stage") ||
        !strcmp(argv[1], "--show") || !strcmp(argv[1], "--save") ||
        !strcmp(argv[1], "--restore"))) {
        if (dump_path || status_mode || memory_mode) return 2;
        return editor_main(argc, argv);
    }
    for (i = start; i < argc; ++i) {
        if (!strcmp(argv[i], "-v")) verbose = 1;
        else if (!strcmp(argv[i], "--details")) details = 1;
        else if (!strcmp(argv[i], "--status")) status_mode = 1;
        else if (!strcmp(argv[i], "--usb") || !strcmp(argv[i], "--disks")) {
            if (status_command) {
                fprintf(stderr, "pmstat: choose one of --usb or --disks\n"); return 2;
            }
            status_command = !strcmp(argv[i], "--usb") ? 0x61 : 0x62;
            status_mode = 1; details = 1;
        }
        else if (!strcmp(argv[i], "--memory")) { if (!memory_mode) memory_mode = 1; }
        else if (!strcmp(argv[i], "--probe")) memory_mode = 2;
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc) {
            dump_path = argv[++i]; details = 1;
        }
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) {
            value = strtoul(argv[++i], &end, 16);
            if (*end || !argv[i][0] || value < 0xc000 || value > 0xe800 ||
                (value & 0x7f)) {
                fprintf(stderr, "pmconfig: segment must be C000..E800, aligned to 0080\n");
                return 2;
            }
            seg = (unsigned)value;
        } else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
            path = argv[++i]; fixture = 1;
        } else {
            printf("Usage: pmconfig [--details] [-v] [-s HEXSEG] [-f MEMORY_IMAGE]\n");
            printf("Default inspection reads /dev/kmem; no firmware or port writes.\n");
            printf("--details: expanded report; known profile includes image directories\n");
            printf("-v: raw config (256 bytes; 768 with --details on a known profile)\n");
            printf("-s: ROM segment  -f: offline physical memory image\n");
            printf("--dump NEWFILE: bounded support bundle (known profile only)\n");
            printf("--status: passive status  --memory: active map query  --probe: BIOS block\n");
            printf("--usb / --disks: explicit bounded firmware status query\n");
            editor_usage();
            return !strcmp(argv[i], "-h") || !strcmp(argv[i], "--help") ? 0 : 2;
        }
    }
    if ((dump_path != NULL) + status_mode + (memory_mode != 0) > 1) {
        fprintf(stderr, "pmconfig: choose one of dump, status or memory\n");
        return 2;
    }
    if ((memory_mode || status_command) && fixture) {
        fprintf(stderr, "pmconfig: queries require live hardware\n"); return 2;
    }
    if (memory_mode) details = 1;
    fd = open(path, O_RDONLY);
    if (fd < 0) { perror(path); return 1; }
    if (seg) found = rom_at(seg) ? 1 : 0;
    else {
        for (candidate = 0xc000; candidate <= 0xe800; candidate += 0x80) {
            if (rom_at(candidate)) { seg = candidate; ++found; }
        }
    }
    if (found != 1) {
        fprintf(stderr, "pmconfig: %u PicoMEM ROMs found; expected one%s\n",
                found, found > 1 ? "; select with -s" : "");
        close(fd); return 1;
    }
    if (!rom_at(seg)) { close(fd); return 1; }
    for (attempt = 0; attempt < 3; ++attempt) {
        unsigned long base = ((unsigned long)seg << 4) + 0x4000;
        if (!read_at(base, snap, sizeof(snap)) ||
            !read_at(base, check, sizeof(check))) {
            fprintf(stderr, "pmconfig: cannot read shared memory\n");
            close(fd); return 1;
        }
        /* BIOS counters/register scratch may change; require stable config. */
        if (snap[0] != 0x12 || check[0] != 0x12 ||
            memcmp(snap + CFG, check + CFG, 256)) continue;
        extended = details && profile_ok();
        if (extended) {
            captured = capture_details(base);
            if (captured < 0) {
                fprintf(stderr, "pmconfig: cannot read full configuration profile\n");
                close(fd); return 1;
            }
            if (!captured) continue;
        }
        break;
    }
    close(fd);
    if (attempt == 3) {
        fprintf(stderr, "pmconfig: shared ID invalid or configuration unstable\n");
        return 1;
    }
    if (dump_path || memory_mode || status_command) {
        if (!extended) {
            fprintf(stderr, "pmconfig: operation requires board 11 / BIOS 2026-06-16, healthy SD/config\n");
            return 1;
        }
        if (status_command) return report_live_status(seg, status_command);
        if (dump_path) return write_dump(dump_path, seg, fixture);
        return report_memory(seg, memory_mode == 2);
    }
    if (status_mode) { report_status(); return 0; }
    report(seg, fixture, extended);
    if (details) report_details(extended);
    if (verbose) report_raw(extended);
    return 0;
}
