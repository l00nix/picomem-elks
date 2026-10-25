/* SPDX-License-Identifier: GPL-2.0-or-later
 * Bounded support record: header 32, ROM identity 128, BIOS vars 32,
 * validated config 768, CRC32 4. No command parameters or arbitrary memory.
 * Reuses editor record buffers, because these modes are mutually exclusive.
 */
#define DUMP_CRC 960
#define DUMP_BYTES 964
static int write_dump(const char *path, unsigned seg, int fixture)
{
    unsigned i, done = 0;
    unsigned long crc;
    int out, n, ok = 1;
    unsigned char extra;
    memset(record, 0, DUMP_BYTES);
    memcpy(record, "PMDMP01\n", 8);
    putword(record + 8, seg);
    putword(record + 10, CONFIG_BYTES);
    record[12] = fixture ? 1 : 0;
    memcpy(record + 32, hdr, 128);
    memcpy(record + 160, snap, 32);
    memcpy(record + 192, livecfg, CONFIG_BYTES);
    crc = crc32_bytes(record, DUMP_CRC);
    for (i = 0; i < 4; ++i) record[DUMP_CRC+i] = (unsigned char)(crc >> (8*i));
    out = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (out < 0) { perror(path); return 1; }
    while (done < DUMP_BYTES) {
        n = write(out, record + done, DUMP_BYTES - done);
        if (n <= 0) { ok = 0; break; }
        done += n;
    }
    if (close(out) < 0) ok = 0;
    sync();
    if (!ok) goto failed;
    out = open(path, O_RDONLY);
    if (out < 0) goto failed;
    done = 0;
    while (done < DUMP_BYTES) {
        n = read(out, original + done, DUMP_BYTES - done);
        if (n <= 0) break;
        done += n;
    }
    n = read(out, &extra, 1);
    if (close(out) < 0) ok = 0;
    if (!ok || done != DUMP_BYTES || n != 0 || memcmp(record, original, DUMP_BYTES))
        goto failed;
    printf("PMDUMP " PMCFG_VERSION " - support snapshot%s\n", fixture ? " [FILE]" : "");
    printf("Saved and verified %u bytes: %s\n", DUMP_BYTES, path);
    printf("ROM %04X  board %u  configuration %u bytes\n", seg, snap[27], CONFIG_BYTES);
    printf("Includes ROM identity, sampled BIOS variables and configuration.\n");
    printf("Excludes command parameters, WiFi keys and unrelated memory.\n");
    printf("Names and paths may be personal; review before sharing.\n");
    printf("Support file only; not a restore file or SD configuration backup.\n");
    return 0;
failed:
    fprintf(stderr, "pmdump: output verification failed; partial file retained\n");
    return 1;
}

static void report_status(void)
{
    printf("PMSTAT " PMCFG_VERSION " - sampled status, no firmware commands\n");
    printf("Board %u  primary IRQ %u  hardware IRQ mask %02X\n", snap[27], snap[11], snap[12]);
    printf("SD: %s [%02X]  Config: %s [%02X]\n", state(snap[5]), snap[5], state(snap[7]), snap[7]);
    printf("USB init: %s [%02X]  WiFi init: %s [%02X]\n", state(snap[6]), snap[6], state(snap[8]), snap[8]);
    printf("NE2000 configured: port %03X  IRQ %u\n", word(snap + CFG + 216), snap[CFG+215]);
    printf("BIOS disks: HDD %u -> %u  FDD %u -> %u\n", snap[17], snap[21], snap[18], snap[22]);
    printf("USB configured: %s  BIOS starts %u  Bootstraps %u\n", yesno(snap[CFG+210]), snap[2], snap[3]);
    printf("Initialization flags do not prove a current connection or mount.\n");
    printf("Use --usb or --disks for an explicit firmware status query.\n");
    printf("No WiFi query, reconnect, keys, reset or configuration changes.\n");
}
