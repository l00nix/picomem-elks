#!/usr/bin/env python3
"""Install optional mouse + PicoMEM filesystem into an exact ELKS checkout.
Preflight all integration edits before writing. Never edits .config or boot files.
"""
from pathlib import Path
import subprocess, sys
BASE = 'e7a56a4334e0ef9a2261d1c2de54db31ba40fdf0'
root = Path(sys.argv[1]).resolve()
kit = Path(__file__).resolve().parent
head = subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
if head != BASE:
    raise SystemExit('Expected ELKS '+BASE+'; no edits made')
changes = {}
def edit(name, old, new, marker=None):
    p = root/name
    s = changes.get(p, p.read_text())
    if new in s or (marker and marker in s): return
    if s.count(old) != 1: raise SystemExit('Unexpected source '+name+'; no edits made')
    changes[p] = s.replace(old,new)
char = 'elks/arch/i86/drivers/char/'
edit(char+'Makefile','OBJS  = init.o ntty.o','OBJS  = init.o ntty.o\nifdef CONFIG_MOUSE_PICOMEM\nOBJS += mouse-picomem.o\nendif')
edit(char+'config.in',"\tbool 'Pseudo tty device driver'\t\tCONFIG_PSEUDO_TTY\t  y", "\tbool 'Pseudo tty device driver'\t\tCONFIG_PSEUDO_TTY\t  y\n\tif [ \"$CONFIG_ARCH_IBMPC\" = \"y\" ]; then\n\t\tbool 'Experimental PicoMEM mouse (PPC512)' CONFIG_MOUSE_PICOMEM n\n\tfi")
edit('elks/include/linuxmt/ntty.h','#define MAX_TTYS (NR_CONSOLES+NR_SERIAL+NR_PTYS+NR_MOUSE)', '#ifdef CONFIG_MOUSE_PICOMEM\n#define NR_PMOUSE 1\n#else\n#define NR_PMOUSE 0\n#endif\n#define MAX_TTYS (NR_CONSOLES+NR_SERIAL+NR_PTYS+NR_MOUSE+NR_PMOUSE)')
edit(char+'ntty.c','extern struct tty_ops ps2_mouse_ops;', 'extern struct tty_ops picomem_mouse_ops;\nextern struct tty_ops ps2_mouse_ops;')
edit(char+'ntty.c','    register_chrdev(TTY_MAJOR, "tty", &tty_fops);', '#ifdef CONFIG_MOUSE_PICOMEM\n        ttyp->ops = &picomem_mouse_ops;\n        (ttyp++)->minor = 33;  /* /dev/pmouse: major 4, minor 33 */\n#endif\n\n    register_chrdev(TTY_MAJOR, "tty", &tty_fops);')
edit('elks/fs/Makefile','ifdef CONFIG_EXEC_COMPRESS','ifdef CONFIG_FS_PICOMEM\nOBJS += picofs.o\nendif\n\nifdef CONFIG_EXEC_COMPRESS')
edit('elks/fs/config.in',"\tcomment 'Filesystem settings'", "\tif [ \"$CONFIG_ARCH_IBMPC\" = \"y\" ]; then\n\t    bool 'Experimental read-only PicoMEM USB/SD filesystem' CONFIG_FS_PICOMEM n\n\tfi\n\n\tcomment 'Filesystem settings'")
edit('elks/fs/config.in', "\tbool 'External buffer cache'           CONFIG_FS_EXTERNAL_BUFFER  y", '\tif [ "$CONFIG_FS_PICOMEM" = "y" ]; then\n\t    bool \'Experimental verified USB writes (SD read-only)\' CONFIG_FS_PICOMEM_WRITE n\n\tfi\n\n\tbool \'External buffer cache\'           CONFIG_FS_EXTERNAL_BUFFER  y', marker='CONFIG_FS_PICOMEM_WRITE n')
edit('elks/include/linuxmt/fs.h','#define FST_ROMFS       3','#define FST_ROMFS       3\n#define FST_PICOFS      4\n\n#ifdef __KERNEL__\n#ifdef CONFIG_FS_PICOMEM\nextern int picofs_mount(char *, char *, int);\nextern int picofs_busy(void);\n#endif\nextern int do_mount(kdev_t, char *, int, int, char *);\n#endif', marker='#define FST_PICOFS      4')
edit('elks/fs/super.c','/* Supported File System List */','/* Supported File System List */\n#ifdef CONFIG_FS_PICOMEM\nextern struct file_system_type picofs_fs_type;\n#endif')
edit('elks/fs/super.c','        &msdos_fs_type,\n#endif\n        NULL','        &msdos_fs_type,\n#endif\n#ifdef CONFIG_FS_PICOMEM\n        &picofs_fs_type,\n#endif\n        NULL')
edit('elks/fs/super.c','NULL, "minix", "msdos", "romfs"','NULL, "minix", "msdos", "romfs", "picofs"')
edit('elks/fs/super.c','    retval = namei(dev_name, &inode, 0, 0);', '#ifdef CONFIG_FS_PICOMEM\n    if (fstype == &picofs_fs_type)\n        return picofs_mount(dev_name, dir_name, flags);\n#endif\n\n    retval = namei(dev_name, &inode, 0, 0);')
edit('elks/include/linuxmt/fs.h', '#define FST_MINIX       1', '#ifdef __KERNEL__\n#ifdef CONFIG_FS_PICOMEM_WRITE\nstruct inode;\nextern int picofs_truncate(struct inode *, unsigned long);\n#endif\n#endif\n#define FST_MINIX       1')
edit('elks/fs/namei.c', '        if (flag & O_TRUNC) {\n            inode->i_size = 0L;', '''        if (flag & O_TRUNC) {
#ifdef CONFIG_FS_PICOMEM_WRITE
            if (inode->i_sb->s_type->type == FST_PICOFS) {
                error = picofs_truncate(inode, 0);
                if (error) { iput(inode); goto onamei_end; }
            } else
#endif
            {
            inode->i_size = 0L;''')
edit('elks/fs/namei.c', '            inode->i_dirt = 1;\n        }\n        *res_inode = inode;', '            inode->i_dirt = 1;\n            }\n        }\n        *res_inode = inode;')
# Extend the normal mount utility as well as the kernel. Older binaries have
# an unchecked type-name array and must be replaced before enabling reporting.
edit('elkscmd/sys_utils/mount.c', '0, "minix", "msdos", "romfs"', '0, "minix", "msdos", "romfs", "picofs"')
edit('elkscmd/sys_utils/mount.c', '\tstruct statfs statfs;', '\tstruct statfs statfs;\n\tconst char *name, *typename;')
edit('elkscmd/sys_utils/mount.c', '\tif (statfs.f_type < FST_MSDOS) ', '''	/* PicoMEM volumes have no /dev block nodes. Do not query firmware here.
	 * l00nix, with AI coding assistance. */
	typename = (statfs.f_type > 0 && statfs.f_type <
	    (int)(sizeof(fs_typename)/sizeof(fs_typename[0]))) ?
	    fs_typename[statfs.f_type] : "unknown";
	if (statfs.f_type == FST_PICOFS) {
		name = statfs.f_dev == 1 ? "picomem-usb" :
		    statfs.f_dev == 2 ? "picomem-sd" : "picomem";
		printf("%s (%s, %s) mount %s\\n", name, typename,
		    statfs.f_flags & MS_RDONLY ? "ro" : "rw", statfs.f_mntonname);
		return 0;
	}
	name = devname(statfs.f_dev, S_IFBLK);
	if (!name) name = "unknown";
	if (statfs.f_type < FST_MSDOS) ''')
edit('elkscmd/sys_utils/mount.c',
     'devname(statfs.f_dev, S_IFBLK), fs_typename[statfs.f_type], statfs.f_blocks,',
     'name, typename, statfs.f_blocks,')
edit('elkscmd/sys_utils/mount.c',
     'devname(statfs.f_dev, S_IFBLK), fs_typename[statfs.f_type], statfs.f_mntonname);',
     'name, typename, statfs.f_mntonname);')
# Migrate the old label before the idempotent option insertion below.
lfn_config = root/'elks/fs/config.in'
lfn_text = changes.get(lfn_config, lfn_config.read_text())
if 'Experimental paired-firmware long names (RO mounts)' in lfn_text:
    changes[lfn_config] = lfn_text.replace('Experimental paired-firmware long names (RO mounts)',
                                          'Experimental paired-firmware long names')
edit('elks/fs/config.in', "\t    bool 'Experimental verified USB writes (SD read-only)' CONFIG_FS_PICOMEM_WRITE n", "\t    bool 'Experimental verified USB writes (SD read-only)' CONFIG_FS_PICOMEM_WRITE n\n\t    bool 'Experimental paired-firmware long names' CONFIG_FS_PICOMEM_LFN n", marker='CONFIG_FS_PICOMEM_LFN n')
edit('elks/fs/config.in', "\t    bool 'Experimental paired-firmware long names' CONFIG_FS_PICOMEM_LFN n", "\t    bool 'Experimental paired-firmware long names' CONFIG_FS_PICOMEM_LFN n\n\t    if [ \"$CONFIG_FS_PICOMEM_LFN\" = \"y\" -a \"$CONFIG_FS_PICOMEM_WRITE\" = \"y\" ]; then\n\t        bool 'Experimental paired-firmware USB long-name creation' CONFIG_FS_PICOMEM_LFN_CREATE n\n\t    fi", marker='CONFIG_FS_PICOMEM_LFN_CREATE n')
edit('elks/fs/config.in', "\t        bool 'Experimental paired-firmware USB long-name creation' CONFIG_FS_PICOMEM_LFN_CREATE n", "\t        bool 'Experimental paired-firmware USB long-name creation' CONFIG_FS_PICOMEM_LFN_CREATE n\n\t        if [ \"$CONFIG_FS_PICOMEM_LFN_CREATE\" = \"y\" ]; then\n\t            bool 'Experimental same-parent USB file rename' CONFIG_FS_PICOMEM_LFN_RENAME n\n\t        fi")
edit('elks/include/linuxmt/fs.h', '#define FST_PICOFS      4', '#define FST_PICOFS      4\n#ifdef __KERNEL__\n#ifdef CONFIG_FS_PICOMEM_LFN_RENAME\nstruct inode;\nextern int picofs_rename(struct inode *,const char *,size_t,struct inode *,const char *,size_t);\n#endif\n#endif')
edit('elks/fs/namei.c', 'int sys_rename(register char *oldname, char *pathname)', '#include "namei-picomem.inc"\n\nint sys_rename(register char *oldname, char *pathname)')
edit('elks/fs/namei.c', '    return !(err = sys_link(oldname, pathname)) ? sys_unlink(oldname) : err;', '    #ifdef CONFIG_FS_PICOMEM_LFN_RENAME\n    int handled;\n    err = rename_picofs_path(oldname, pathname, &handled);\n    if (handled) return err;\n    #endif\n    return !(err = sys_link(oldname, pathname)) ? sys_unlink(oldname) : err;')
for name in ('picofs-rename.inc','namei-picomem.inc'):
    changes[root/'elks/fs'/name] = (kit/name).read_text()
for name in ('mouse-picomem.c','pmmouse.h'):
    changes[root/char/name] = (kit.parent/'mouse'/name).read_text()
changes[root/'elks/fs/picofs-create.inc'] = (kit/'picofs-create.inc').read_text()
changes[root/'elks/fs/picofs.c'] = (kit/'picofs.c').read_text()
changes[root/'elks/fs/picofs-lfn.inc'] = (kit/'picofs-lfn.inc').read_text()
changes[root/'elks/fs/picofs-write.inc'] = (kit/'picofs-write.inc').read_text()
edit('elks/fs/config.in', "\t    bool 'Experimental paired-firmware long names' CONFIG_FS_PICOMEM_LFN n",
     "\t    bool 'Experimental paired-firmware long names' CONFIG_FS_PICOMEM_LFN n\n\t    bool 'Experimental paired-firmware timing/stack diagnostics' CONFIG_FS_PICOMEM_DIAG n",
     marker='CONFIG_FS_PICOMEM_DIAG n')
arch='elks/arch/i86/kernel/'
edit(arch+'Makefile', 'ifeq ($(CONFIG_ARCH_IBMPC), y)', 'ifdef CONFIG_FS_PICOMEM_DIAG\nOBJS += picostack.o\nendif\n\nifeq ($(CONFIG_ARCH_IBMPC), y)')
edit(arch+'irqtab.S', '#ifdef CONFIG_TRACE\n        call    trace_begin', '#if defined(CONFIG_TRACE) || defined(CONFIG_FS_PICOMEM_DIAG)\n        call    trace_begin')
edit(arch+'strace.c', '#include <arch/segment.h>', '''#include <arch/segment.h>
#ifdef CONFIG_FS_PICOMEM_DIAG
#ifdef CONFIG_TRACE
#error PicoMEM diagnostics use separate lightweight stack tracing
#endif
extern void FARPROC picostack_begin(void);
extern void FARPROC picostack_end(void);
void trace_begin(void) { picostack_begin(); }
#endif''')
edit(arch+'strace.c', 'void trace_end(unsigned int retval)\n{', 'void trace_end(unsigned int retval)\n{\n#ifdef CONFIG_FS_PICOMEM_DIAG\n    picostack_end();\n#endif')
changes[root/arch/'picostack.c']=(kit/'picostack.c').read_text()
changes[root/'elks/fs/picofs-diag.inc']=(kit/'picofs-diag.inc').read_text()
for p,s in changes.items(): p.write_text(s)
print('Installed optional CONFIG_MOUSE_PICOMEM + CONFIG_FS_PICOMEM. No boot files changed.')
