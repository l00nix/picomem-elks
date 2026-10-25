/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PMMOUSE_H
#define PMMOUSE_H
#define PM_ARM   0x5041
#define PM_STOP  0x5053
#define PM_STATS 0x5054
struct pm_stats {
    unsigned short interrupts, reports, packets, dropped, unexpected, acks;
    unsigned short errors, armed, fault;
};
#endif
