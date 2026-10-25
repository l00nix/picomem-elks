/* SPDX-License-Identifier: GPL-2.0-or-later
 * Included by pmcfg.c after editor.c. Memory-only extended inspection.
 * Viewer and editor are mutually exclusive; reuse their full-config buffers.
 */
static int capture_details(unsigned long base)
{
    if (!read_at(base + CFG, livecfg, CONFIG_BYTES) ||
        !read_at(base + CFG, verifycfg, CONFIG_BYTES) ||
        !read_at(base, check, sizeof(check)) ||
        !read_at(base - 0x4000, identity, sizeof(identity))) return -1;
    /* Counters may advance. Config and profile identity must remain stable. */
    return check[0] == snap[0] && check[27] == snap[27] &&
           check[5] == snap[5] && check[7] == snap[7] &&
           !memcmp(hdr, identity, sizeof(hdr)) &&
           !memcmp(livecfg, verifycfg, CONFIG_BYTES) &&
           !memcmp(livecfg, snap + CFG, 256) &&
           !memcmp(livecfg, check + CFG, 256);
}

static void audio_port(const char *name, unsigned off)
{
    unsigned port = word(snap + CFG + off);
    printf("  %-12s %s  configured port 0x%04X\n",
           name, port ? "on " : "off", port);
}

static void report_details(int extended)
{
    unsigned i, off, blocks, addr;
    char name[65];
    const unsigned char *c = snap + CFG;
    printf("\nBoard and BIOS variables (sampled; not live ELKS driver status):\n");
    printf("  Board ID: %u  Pico ID: %u\n", snap[27], snap[28]);
    if (!word(snap + 30)) printf("  Firmware revision: not reported (zero)\n");
    else printf("  Firmware revision: 0x%04X\n", word(snap + 30));
    printf("  BIOS starts: %u  Bootstraps: %u  Base port init: %s [%02X]\n",
           snap[2], snap[3], state(snap[9]), snap[9]);
    printf("  BIOS disk counts: HDD %u -> %u  FDD %u -> %u\n",
           snap[17], snap[21], snap[18], snap[22]);
    printf("  BIOS PC memory: %u KB  Tandy mode: %u  RAM blocks: %u\n",
           word(snap + 19), snap[23], snap[24]);
    printf("  Command sample: %02X  Source: %02X  SPI PSRAM flag: %u\n",
           snap[25], snap[26], snap[15]);
    printf("  USB device bits: %02X (unused; not a device inventory)\n", snap[10]);
    printf("\nConfigured option ROMs (not proof of activation):\n");
    for (i = 0; i < 2; ++i) {
        off = 102 + i * 16;
        clean_name(name, c + off + 2, 14);
        blocks = c[136 + i]; addr = c[134 + i];
        printf("  ROM%u %-14s size %u KB  blocks %u x 16 KB\n",
               i, name[0] ? name : "-", word(c + off), blocks);
        /* DOS PMCFG displays the address byte followed by three hex zeros. */
        printf("       address byte %02X -> segment %04X:0000%s\n",
               addr, addr << 8, blocks ? "" : " (no blocks assigned)");
    }
    printf("\nAdditional configured settings:\n");
    printf("  Ignore A/B segments: %s  BIOS SRAM limit: %u x 16 KB\n",
           yesno(c[209]), c[220]);
    printf("  BIOS ROM limit: %u x 16 KB  Setup before boot: %s\n",
           c[221], yesno(c[222]));
    printf("  BIOS boot flag: %u  SD clock: %u MHz  RAM clock: %u MHz\n",
           c[223], c[211], c[212]);
    printf("  RTC: %s (0x2C0-0x2C7)  Menu colour profile: %u\n",
           yesno(c[241]), c[240]);
    printf("\nConfigured audio (not an audio probe):\n");
    printf("  Rendering: %s  AdLib: %s (0x388-0x389)\n",
           yesno(c[224]), yesno(c[229]));
    audio_port("Tandy", 230); audio_port("CMS", 232);
    audio_port("SoundBlaster", 234); audio_port("Mindscape", 238);
    printf("  SoundBlaster IRQ: %u (only relevant when enabled)\n", c[236]);
    printf("  Other audio bytes: %02X %02X %02X %02X %02X\n",
           c[225], c[226], c[227], c[228], c[237]);
    if (!extended) {
        printf("\nExtended fields unavailable: need board 11 / BIOS 2026-06-16\n");
        printf("and healthy SD/config init. No layout query was issued.\n");
        return;
    }
    printf("\nExtended image settings (names/paths, not mount status):\n");
    clean_name(name, livecfg + 246, 14);
    printf("  CD-ROM: %s  stored size word: %u\n",
           name[0] ? name : "-", word(livecfg + 244));
    for (i = 0; i < 6; ++i) {
        clean_name(name, livecfg + 260 + i * 64, 64);
        printf("  %s%u directory:\n", i < 2 ? "FDD" : "HDD", i < 2 ? i : i - 2);
        printf("    %s\n", name[0] ? name : "(empty)");
    }
}

static void report_raw(int extended)
{
    unsigned i, len = extended ? CONFIG_BYTES : 256;
    const unsigned char *c = extended ? livecfg : snap + CFG;
    printf("\nRaw %s (%u bytes):\n",
           extended ? "profile configuration" : "common configuration prefix", len);
    for (i = 0; i < len; ++i) {
        if (!(i & 15)) printf("%03X:", i);
        printf(" %02X", c[i]);
        if ((i & 15) == 15) putchar('\n');
    }
}
