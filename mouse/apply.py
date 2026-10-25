#!/usr/bin/env python3
"""Apply the experimental driver to a clean, pinned ELKS e7a56a43 checkout."""
import pathlib, subprocess, sys, shutil
src=pathlib.Path(__file__).resolve().parent
root=pathlib.Path(sys.argv[1]).resolve()
head=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
if not head.startswith('e7a56a43'): raise SystemExit('Expected ELKS e7a56a43; no edits made')
def edit(name,old,new):
    p=root/name;s=p.read_text()
    if new in s:return
    if s.count(old)!=1:raise SystemExit('Unexpected source: '+name)
    p.write_text(s.replace(old,new))
base='elks/arch/i86/drivers/char/'
for name in ('mouse-picomem.c','pmmouse.h'):shutil.copyfile(src/name,root/base/name)
edit(base+'Makefile','OBJS  = init.o ntty.o','OBJS  = init.o ntty.o\nifdef CONFIG_MOUSE_PICOMEM\nOBJS += mouse-picomem.o\nendif')
edit(base+'config.in',"\tbool 'Pseudo tty device driver'\t\tCONFIG_PSEUDO_TTY\t  y", "\tbool 'Pseudo tty device driver'\t\tCONFIG_PSEUDO_TTY\t  y\n\tif [ \"$CONFIG_ARCH_IBMPC\" = \"y\" ]; then\n\t\tbool 'Experimental PicoMEM mouse (PPC512)' CONFIG_MOUSE_PICOMEM n\n\tfi")
edit('elks/include/linuxmt/ntty.h','#define MAX_TTYS (NR_CONSOLES+NR_SERIAL+NR_PTYS+NR_MOUSE)', '#ifdef CONFIG_MOUSE_PICOMEM\n#define NR_PMOUSE 1\n#else\n#define NR_PMOUSE 0\n#endif\n#define MAX_TTYS (NR_CONSOLES+NR_SERIAL+NR_PTYS+NR_MOUSE+NR_PMOUSE)')
edit(base+'ntty.c','extern struct tty_ops ps2_mouse_ops;', 'extern struct tty_ops picomem_mouse_ops;\nextern struct tty_ops ps2_mouse_ops;')
edit(base+'ntty.c','    register_chrdev(TTY_MAJOR, "tty", &tty_fops);', '#ifdef CONFIG_MOUSE_PICOMEM\n        ttyp->ops = &picomem_mouse_ops;\n        (ttyp++)->minor = 33;  /* /dev/pmouse: major 4, minor 33 */\n#endif\n\n    register_chrdev(TTY_MAJOR, "tty", &tty_fops);')
print('Applied optional CONFIG_MOUSE_PICOMEM; create /dev/pmouse c 4 33 mode 600')
