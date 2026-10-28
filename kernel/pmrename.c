/* SPDX-License-Identifier: GPL-2.0-or-later
 * One rename syscall; no destination unlink or copy/delete fallback.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
int main(int argc,char **argv)
{
 if(argc!=3){fprintf(stderr,"Usage: pmrename OLD NEW\nPicoMEM candidate: same-directory regular files; existing targets refused.\n");return 2;}
 if(rename(argv[1],argv[2])){fprintf(stderr,"pmrename: %s\n",strerror(errno));return 1;}
 return 0;
}
