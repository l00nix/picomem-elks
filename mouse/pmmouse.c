/* SPDX-License-Identifier: GPL-2.0-or-later
 * Bounded capture for the experimental /dev/pmouse kernel driver.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include "pmmouse.h"
static volatile int finished;
static void finish(int sig) { (void)sig; finished = 1; }
int main(int argc, char **argv)
{
    int fd, n, used = 0, run = 0, result = 0, events = 0, dx, dy;
    unsigned char packet[3];
    struct pm_stats stats;
    struct timeval timeout;
    fd_set set;
    time_t deadline;
    if (argc == 2 && !strcmp(argv[1], "--run")) run = 1;
    else if (argc != 2 || strcmp(argv[1], "--check")) {
        fprintf(stderr,"Usage: pmmouse --check | --run\n--run captures 30 seconds; Ctrl-C stops.\n");
        return 2;
    }
    signal(SIGINT, finish); signal(SIGTERM, finish); signal(SIGHUP, finish);
    signal(SIGALRM, finish);
    fd = open("/dev/pmouse", O_RDONLY | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) { perror("/dev/pmouse"); return 1; }
    puts("PMMOUSE experimental - PicoMEM USB input on ELKS");
    puts("Board/ROM/IRQ profile accepted. Open does not enable reporting.");
    if (!run) goto stats;
    if (ioctl(fd, PM_ARM, 0) < 0) { perror("arm"); result = 1; goto stop; }
    puts("IRQ7 claimed. Move the mouse and click buttons; capture lasts 30 seconds.");
    fflush(stdout);
    alarm(30); deadline = time(NULL) + 30;
    while (!finished && time(NULL) < deadline) {
        FD_ZERO(&set); FD_SET(fd, &set);
        timeout.tv_sec = 1; timeout.tv_usec = 0;
        n = select(fd+1, &set, NULL, NULL, &timeout);
        if (n < 0) { if (errno == EINTR) continue; perror("select"); result = 1; break; }
        if (!n) continue;
        n = read(fd, packet+used, 3-used);
        if (n < 0) { if (errno == EAGAIN || errno == EINTR) continue; perror("read"); result = 1; break; }
        used += n;
        if (used != 3) continue;
        dx = (signed char)packet[1]; dy = -(int)(signed char)packet[2];
        ++events;
        /* Bound serial/terminal output under rapid movement. */
        if (events <= 100)
            printf("%3d: dx=%4d dy=%4d buttons=%u  [%02X %02X %02X]\n",
                events,dx,dy,packet[0]&7,packet[0],packet[1],packet[2]);
        used = 0;
    }
stop:
    alarm(0);
    if (ioctl(fd, PM_STOP, 0) < 0) { perror("stop (reboot required)"); result = 1; }
    else puts("Reporting stopped; IRQ7 released.");
stats:
    if (ioctl(fd, PM_STATS, &stats) < 0) { perror("stats"); result = 1; }
    else {
        printf("IRQ=%u reports=%u packets=%u drops=%u unexpected=%u ack=%u\n",
            stats.interrupts,stats.reports,stats.packets,stats.dropped,stats.unexpected,stats.acks);
        printf("errors=%u armed=%u fault=%u; read=%d packets\n",
            stats.errors,stats.armed,stats.fault,events);
        if (stats.errors || stats.fault || stats.unexpected) result = 1;
    }
    close(fd);
    return result;
}
