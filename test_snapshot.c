/* SPDX-License-Identifier: GPL-2.0-or-later
 * Model changing shared memory between reads; never opens a hardware device.
 */
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
static ssize_t snapshot_read(int f, void *p, size_t n);
static off_t snapshot_seek(int f, off_t pos, int whence);
#define read snapshot_read
#define lseek snapshot_seek
#define main pmcfg_main
#include "pmcfg.c"
#undef main
#undef read
#undef lseek

static unsigned phase;
static int change_phase, change_offset, fail_phase;
static unsigned char baseline[CONFIG_BYTES];

static off_t snapshot_seek(int f, off_t pos, int whence)
{
    (void)f;
    assert(whence == SEEK_SET);
    assert(pos == (phase < 2 ? 0xD4052 : phase == 2 ? 0xD4000 : 0xD0000));
    return pos;
}

static ssize_t snapshot_read(int f, void *p, size_t n)
{
    (void)f;
    assert(phase < 4);
    if ((int)phase == fail_phase) return -1;
    assert(n == (phase < 2 ? CONFIG_BYTES : phase == 2 ? sizeof(snap) : sizeof(hdr)));
    memcpy(p, phase < 2 ? baseline : phase == 2 ? snap : hdr, n);
    if ((int)phase == change_phase) ((unsigned char *)p)[change_offset] ^= 1;
    ++phase;
    return (ssize_t)n;
}

static void reset(void)
{
    memset(snap, 0, sizeof(snap)); memset(hdr, 0, sizeof(hdr));
    memset(baseline, 0x35, sizeof(baseline));
    memcpy(snap + CFG, baseline, 256);
    snap[0] = 0x12; snap[27] = 11;
    phase = 0; change_phase = fail_phase = -1; change_offset = 0;
}

int main(void)
{
    unsigned i;
    const unsigned changes[][2] = {
        {1, 600}, {0, 215}, {2, CFG + 215}, {2, 0},
        {2, 27}, {2, 5}, {2, 7}, {3, 20}
    };
    reset(); assert(capture_details(0xD4000) == 1 && phase == 4);
    for (i = 0; i < sizeof(changes)/sizeof(changes[0]); ++i) {
        reset(); change_phase = changes[i][0]; change_offset = changes[i][1];
        assert(capture_details(0xD4000) == 0 && phase == 4);
    }
    reset(); change_phase = 2; change_offset = 2; /* BIOS counters may advance. */
    assert(capture_details(0xD4000) == 1);
    for (i = 0; i < 4; ++i) {
        reset(); fail_phase = i;
        assert(capture_details(0xD4000) == -1);
    }
    puts("14 snapshot scenarios passed: stable, eight changing identities/configs,");
    puts("changing BIOS counter, and four read failures.");
    return 0;
}
