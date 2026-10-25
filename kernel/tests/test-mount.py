#!/usr/bin/env python3
"""Compile the installed ELKS mount listing code with a synthetic mount table."""
from pathlib import Path
import os, subprocess, sys, tempfile
source=Path(sys.argv[1]).read_text()
listing=source[source.index('static char *fs_typename'):source.index('static int usage')]
shim=r'''
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef unsigned dev_t;
#define FST_MSDOS 2
#define FST_PICOFS 4
#define MS_RDONLY 1
#define UF_NOFREESPACE 1
#define S_IFBLK 0060000
#define NR_SUPER 9
struct statfs {
 int f_type; unsigned f_flags; dev_t f_dev;
 unsigned long f_blocks,f_bfree; char f_mntonname[32];
};
static unsigned queries, lookups;
static int ustatfs(dev_t slot,struct statfs *s,int flags) {
 assert(flags==UF_NOFREESPACE);++queries;memset(s,0,sizeof(*s));
 switch(slot) {
 case 0:s->f_type=1;s->f_dev=0x300;strcpy(s->f_mntonname,"/");s->f_blocks=64000;s->f_bfree=32000;break;
 case 1:s->f_type=4;s->f_dev=1;s->f_flags=1;strcpy(s->f_mntonname,"/media/usb");break;
 case 2:s->f_type=4;s->f_dev=2;s->f_flags=1;strcpy(s->f_mntonname,"/mnt/sd");break;
 case 3:s->f_type=99;s->f_dev=0x999;strcpy(s->f_mntonname,"/other");break;
 case 4:return -1;
 case 5:s->f_type=4;s->f_dev=3;strcpy(s->f_mntonname,"/future");break;
 case 6:s->f_type=2;s->f_dev=0x100;strcpy(s->f_mntonname,"/fat");break;
 case 7:s->f_type=0;s->f_dev=0;strcpy(s->f_mntonname,"/zero");break;
 case 8:s->f_type=-1;s->f_dev=0;strcpy(s->f_mntonname,"/negative");break;
 default:assert(0);
 }
 return 0;
}
static char *devname(dev_t d,int mode) {
 assert(mode==S_IFBLK);++lookups;
 /* PicoMEM must never be looked up as a block device. */
 assert(d!=1 && d!=2 && d!=3);
 return d==0x300 ? "/dev/hda1" : d==0x100 ? "/dev/fd0" : NULL;
}
'''
with tempfile.TemporaryDirectory(prefix='picomem-mount-') as d:
    p=Path(d)/'test.c';exe=Path(d)/'test'
    p.write_text(shim+listing+'\nint main(void) { show(); assert(queries==9 && lookups==5); return 0; }\n')
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
    lines=subprocess.check_output([str(exe)],text=True).splitlines()
    assert len(lines)==8,lines
    assert lines[0].startswith('/dev/hda1') and lines[0].endswith('mount /'),lines
    assert lines[1]=='picomem-usb (picofs, ro) mount /media/usb',lines
    assert lines[2]=='picomem-sd (picofs, ro) mount /mnt/sd',lines
    assert '(unknown)' in lines[3] and lines[3].endswith('mount /other'),lines
    assert lines[4]=='picomem (picofs, rw) mount /future',lines
    assert lines[5].startswith('/dev/fd0') and '(msdos)' in lines[5],lines
    assert '(unknown)' in lines[6] and '(unknown)' in lines[7],lines
print('mount: 9 mount-table cases passed; PicoMEM labels need no block devices')
