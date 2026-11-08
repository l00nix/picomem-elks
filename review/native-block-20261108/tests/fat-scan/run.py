from pathlib import Path
import re,subprocess,sys,tempfile,shutil
original=Path(__file__).resolve().parent
scratch=tempfile.TemporaryDirectory();p=Path(scratch.name)
shutil.copyfile(original/'host-scan.c',p/'host-scan.c')
s=(Path(sys.argv[1])/'elks/fs/msdos/misc.c').read_text();a=s.index('\tlimit = sb->clusters;',s.index('msdos_add_cluster('));b=s.index('\n\tif (count >= limit)',a)
core=re.sub(r'\s*debug_fat\([^\n]+\);','',s[a:b]).replace('unsigned long','uint32_t').replace('unsigned short','uint16_t')
(p/'scan-core.inc').write_text(core)
subprocess.run(['clang','-std=c99','-Wall','-Wextra','-Werror','-O1','-fsanitize=address,undefined',str(p/'host-scan.c'),'-o',str(p/'host-scan')],check=True)
subprocess.run([str(p/'host-scan')],check=True)

subprocess.run(['clang','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-DFAT_BITS_32','-O1','-fsanitize=address,undefined',str(p/'host-scan.c'),'-o',str(p/'host-scan32')],check=True)
subprocess.run([str(p/'host-scan32')],check=True)
