/* SPDX-License-Identifier: GPL-2.0-or-later
 * Experimental PPC512 PicoMEM mouse driver. l00nix, with AI coding assistance.
 * Pinned to board 11, D000 ROM 2026-06-16, primary IRQ7, direct NE2000 IRQ5.
 * IRQ sequence checked against the installed ROM, D000:2C1E..2D90.
 * Opening is passive; PM_ARM explicitly claims IRQ7 and enables reports.
 */
#ifdef PMOUSE_HOST_TEST
#include "tests/host-shim.h"
#else
#include <linuxmt/config.h>
#include <linuxmt/kernel.h>
#include <linuxmt/sched.h>
#include <linuxmt/errno.h>
#include <linuxmt/ntty.h>
#include <linuxmt/segment.h>
#include <linuxmt/mm.h>
#include <string.h>
#include <arch/io.h>
#include <arch/irq.h>
#endif
#include "pmmouse.h"

#define PM_PORT 0x2a0
#define PM_IRQ 7
#define CFG (0x4000 + 82)
#define IRR 0x4356
#define ISR 0x435c
#define PM_IRR 0x435e
#define PM_ISR 0x435f
#define PM_COUNT 0x4360
#define MX 0x4361
#define MY 0x4362
#define MB 0x4363
#define MOUSE_BIT 8
#define POLLS 32768U
#ifndef PMOUSE_HOST_TEST
#define PMMEM ((volatile unsigned char __far *)0xd0000000UL)
#define IVT ((volatile unsigned long __far *)0UL)
#endif

static struct tty *pm_tty;
static unsigned pm_claimed, pm_armed, pm_fault, pm_was_unmasked;
static unsigned long pm_old_vector;
static struct pm_stats pm_stats;

/* Keep optional file-service mounts and mouse capture mutually exclusive.
 * Opening/arming never disables a mounted filesystem. */
int picomem_mouse_busy(void) { return pm_tty != NULL || pm_claimed || pm_fault; }

/* Caller holds CLI. Acknowledge is the fast sideband command 01, not 31.
 * This path must not reset the management command/status registers. */
static int other_sources(void)
{
    return PMMEM[IRR] || PMMEM[IRR+1] || PMMEM[IRR+2] ||
           PMMEM[IRR+4] || PMMEM[IRR+5];
}

static int irq_idle(void)
{
    return !other_sources() && !PMMEM[IRR+3] && !PMMEM[ISR] &&
           !PMMEM[ISR+1] && !PMMEM[PM_IRR] && !PMMEM[PM_ISR] &&
           !PMMEM[PM_COUNT];
}

static unsigned rom_crc(void)
{
    unsigned short crc = 0xffff;
    unsigned i, b;
    for (i = 0x2c1e; i < 0x2d91; ++i) {
        crc ^= (unsigned)PMMEM[i] << 8;
        for (b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

static int profile(void)
{
    unsigned i;
    unsigned char p, n;
    static const char date[] = "(Date 2026-06-16)";
    if (PMMEM[0] != 0x55 || PMMEM[1] != 0xaa || PMMEM[2] != 0x20)
        return -ENODEV;
    for (i = 0; i < sizeof(date)-1; ++i)
        if (PMMEM[28+i] != date[i]) return -ENODEV;
    if (rom_crc() != 0x596f || PMMEM[0x4000] != 0x12 ||
        PMMEM[0x401b] != 11 || PMMEM[0x400b] != 7 ||
        PMMEM[0x400c] != 0xa8 || PMMEM[0x4005] || PMMEM[0x4007] ||
        PMMEM[0x4019] || PMMEM[0x401a]) return -ENODEV;
    /* USB on; direct NIC IRQ5; no Sound Blaster DMA/IRQ or joystick. */
    if (!PMMEM[CFG+210] || PMMEM[CFG+215] != 5 ||
        PMMEM[CFG+216] != 0 || PMMEM[CFG+217] != 3 ||
        PMMEM[CFG+234] || PMMEM[CFG+235] || PMMEM[CFG+218]) return -ENODEV;
    p = inb(PM_PORT+3);
    for (i = 0; i < 16; ++i) {
        n = inb(PM_PORT+3);
        if (n != (unsigned char)(p+1)) return -ENODEV;
        p = n;
    }
    if (inb(PM_PORT) || !irq_idle()) return -EBUSY;
    return 0;
}

/* IRQs must be disabled. Only runtime mouse enable/disable is permitted.
 * Low-byte sentinel makes immediate completion distinguishable from ignore.
 * Never reset/retry an uncompleted command. */
static int mouse_command(unsigned cmd)
{
    unsigned i;
    unsigned char status;
    if (cmd != 0x52 && cmd != 0x53) return -EINVAL;
    if (inb(PM_PORT)) return -EBUSY;
    outb(0xff, PM_PORT+1);
    outb(0, PM_PORT+2);
    outb(cmd, PM_PORT);
    for (i = 0; i < POLLS; ++i) {
        status = inb(PM_PORT);
        if (!status && !inb(PM_PORT+1)) return 0;
        if (status == 2 || status == 3) return -EIO;
    }
    return -EIO;
}

/* ELKS chq_addch_nowakeup unconditionally executes STI. Do not use it
 * inside the PicoMEM handshake: append under the caller's saved CLI state. */
static void queue_byte(struct ch_queue *q, unsigned char c)
{
    q->base[q->head] = c;
    if (++q->head == q->size) q->head = 0;
    ++q->len;
#ifdef PMOUSE_HOST_TEST
    test_queue_hook();
#endif
}

/* USB Y increases downwards, PS/2 Y upwards. Split +128 for ELKS
 * readers which interpret each movement byte as signed 8-bit. */
static void queue_report(unsigned char x, unsigned char y, unsigned char buttons)
{
    struct ch_queue *q = &pm_tty->inq;
    int dy = -(int)(signed char)y;
    unsigned needed = dy == 128 ? 6 : 3;
    unsigned char h = 8 | (buttons & 7);
    if (x & 0x80) h |= 0x10;
    if (dy < 0) h |= 0x20;
    ++pm_stats.reports;
    if (q->size - q->len < needed) { ++pm_stats.dropped; return; }
    queue_byte(q, h);
    queue_byte(q, x);
    queue_byte(q, dy == 128 ? 127 : (unsigned char)dy);
    ++pm_stats.packets;
    if (dy == 128) {
        queue_byte(q, 8 | (buttons & 7));
        queue_byte(q, 0);
        queue_byte(q, 1);
        ++pm_stats.packets;
    }
}

/* CLI across shared-memory handshake. Generic ELKS IRQ wrapper owns PIC EOI.
 * No BIOS call, print, blocking I/O or management command in the handler. */
static void service_mouse(void)
{
    unsigned count, flags;
    unsigned char x, y, b;
    save_flags(flags); clr_irq();
    if (pm_fault) goto done;
    if (other_sources() || PMMEM[ISR] || PMMEM[ISR+1] || PMMEM[PM_COUNT])
        goto unexpected;
    PMMEM[PM_ISR] = 1;
    ++PMMEM[PM_COUNT];
    PMMEM[PM_IRR] = 0;
    for (count = 0; count < 32; ++count) {
        if (other_sources()) goto fault_count;
        if (!PMMEM[IRR+3]) break;
        if (PMMEM[IRR+3] != 2) goto fault_count;
        PMMEM[PM_IRR] = 0;
        PMMEM[ISR] |= MOUSE_BIT;
        PMMEM[IRR+3] = 0;
        x = PMMEM[MX]; y = PMMEM[MY]; b = PMMEM[MB];
        if (pm_armed && pm_tty) queue_report(x, y, b);
        PMMEM[ISR] &= ~MOUSE_BIT;
    }
    /* Bound a flood; preserve pending state for diagnosis, mask IRQ7. */
    if (PMMEM[IRR+3] || other_sources()) goto fault_count;
    --PMMEM[PM_COUNT];
    outb(1, PM_PORT);
    ++pm_stats.acks;
    goto done;
fault_count:
    --PMMEM[PM_COUNT];
unexpected:
    ++pm_stats.unexpected;
    pm_fault = 1;
    disable_irq(PM_IRQ);
done:
    restore_flags(flags);
    if (pm_tty && pm_tty->inq.len) wake_up(&pm_tty->inq.wait);
}

static void pm_interrupt(int irq, struct pt_regs *regs)
{
    (void)irq; (void)regs;
    ++pm_stats.interrupts;
    service_mouse();
}

static void release_irq(void)
{
    free_irq(PM_IRQ); /* leaves line masked */
    int_vector_set(15, (unsigned)pm_old_vector, (unsigned)(pm_old_vector >> 16));
    if (pm_was_unmasked) enable_irq(PM_IRQ);
    pm_claimed = 0;
}

static int stop_mouse(void)
{
    int err = 0;
    unsigned flags;
    save_flags(flags); clr_irq();
    if (pm_claimed) {
        disable_irq(PM_IRQ);
        if (pm_armed) err = mouse_command(0x53);
        if (!err) {
            pm_armed = 0;
            /* Drain only our source once reporting has definitely stopped. */
            if (!pm_fault) service_mouse();
            if (irq_idle()) release_irq();
            else err = -EIO;
        }
        if (err) {
            pm_fault = 1;
            ++pm_stats.errors;
            /* Retain handler/queue with IRQ7 masked. Reboot is recovery. */
        }
    }
    restore_flags(flags);
    return err;
}

static int arm_mouse(void)
{
    unsigned flags;
    int err;
#ifdef CONFIG_FS_PICOMEM
    if (picofs_busy()) return -EBUSY;
#endif
    save_flags(flags); clr_irq();
    if (pm_claimed || pm_fault) { err = -EBUSY; goto done; }
    err = profile();
    if (err) goto done;
    /* PicoMEM leaves its own BIOS handler unmasked on this PPC. Accept
     * only that ROM-verified handler; request_irq still checks ELKS ownership.
     * Preserve the original mask bit and vector for successful release. */
    pm_old_vector = IVT[15];
    pm_was_unmasked = !(inb(0x21) & 0x80);
    if (pm_was_unmasked && pm_old_vector != 0xd0002c1eUL) {
        err = -EBUSY; goto done;
    }
    err = request_irq(PM_IRQ, pm_interrupt, INT_GENERIC);
    if (err) goto done;
    pm_claimed = 1;
    pm_armed = 1; /* timeout is ambiguous: retain ownership if command fails */
    err = mouse_command(0x52);
    if (err) {
        disable_irq(PM_IRQ);
        pm_fault = 1;
        ++pm_stats.errors;
    }
done:
    restore_flags(flags);
    return err;
}

static int pm_open(struct tty *tty)
{
    int err;
    unsigned flags;
    if (current->euid) return -EPERM;
#ifdef CONFIG_FS_PICOMEM
    if (picofs_busy()) return -EBUSY;
#endif
    if (pm_tty || pm_fault) return -EBUSY;
    save_flags(flags); clr_irq();
    err = profile();
    restore_flags(flags);
    if (err) return err;
    err = tty_allocq(tty, 192, 0);
    if (err) return err;
    memset(&pm_stats, 0, sizeof(pm_stats));
    tty->termios.c_iflag = tty->termios.c_oflag = tty->termios.c_lflag = 0;
    tty->termios.c_cc[VMIN] = 1;
    tty->termios.c_cc[VTIME] = 0;
    tty->usecount = 1;
    pm_tty = tty;
    return 0;
}

static void pm_release(struct tty *tty)
{
    if (stop_mouse()) {
        printk("pmouse: stop failed; IRQ7 masked, reboot required\n");
        return;
    }
    tty->usecount = 0;
    tty_freeq(tty);
    pm_tty = NULL;
}

static int pm_write(struct tty *tty) { (void)tty; return 0; }
static int pm_ioctl(struct tty *tty, int cmd, char *arg)
{
    struct pm_stats snapshot;
    unsigned flags;
    (void)tty;
    switch (cmd) {
    case TCSETS: case TCSETSW: case TCSETSF: return 0;
    case PM_ARM: return current->euid ? -EPERM : arm_mouse();
    case PM_STOP: return stop_mouse();
    case PM_STATS:
        save_flags(flags); clr_irq();
        snapshot = pm_stats;
        snapshot.armed = pm_armed;
        snapshot.fault = pm_fault;
        restore_flags(flags);
        return verified_memcpy_tofs(arg, &snapshot, sizeof(snapshot));
    }
    return -EINVAL;
}

struct tty_ops picomem_mouse_ops = {
    pm_open, pm_release, pm_write, NULL, pm_ioctl, NULL
};
