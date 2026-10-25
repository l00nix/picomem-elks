/* SPDX-License-Identifier: GPL-2.0-or-later
 * PicoMEM mount helper. l00nix, with AI coding assistance.
 * ELKS mount syscall takes a numeric filesystem type, not a string.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#define FST_PICOFS 4
int main(int argc,char **argv)
{
    int rw=0;
    if (argc==4 && !strcmp(argv[1],"--rw")) {rw=1;argc--;argv++;}
    if (argc!=3 || (strcmp(argv[1],"usb") && strcmp(argv[1],"sd") &&
                   strcmp(argv[1],"-u")) || (rw && strcmp(argv[1],"usb"))) {
        fprintf(stderr,"Usage: pmmount usb|sd DIRECTORY\n       pmmount --rw usb DIRECTORY\n       pmmount -u DIRECTORY\nDefault read-only; --rw requires the optional USB-write kernel.\nSD writes are not supported.\n");
        return 2;
    }
    if (getuid()) { fprintf(stderr,"pmmount: root required\n"); return 1; }
    if ((!strcmp(argv[1],"-u") ? umount(argv[2]) :
         mount(argv[1],argv[2],FST_PICOFS,rw?0:MS_RDONLY)) < 0) {
        perror("pmmount"); return 1;
    }
    printf("%s %s\n",!strcmp(argv[1],"-u")?"Unmounted":
        rw?"Mounted USB read/write at":"Mounted read-only at",argv[2]);
    return 0;
}
