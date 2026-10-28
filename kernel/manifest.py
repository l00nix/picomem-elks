#!/usr/bin/env python3
"""Record source/config/artifact hashes and enforce 16-bit segment budgets."""
from pathlib import Path
import hashlib, json, subprocess, sys, re
root, out = map(Path,sys.argv[1:])
full=(root/'elks/arch/i86/boot/system-full.map').read_text()
sizes={}
for section in ('text','fartext','data','bss'):
    m=re.search(r'^\.'+section+r'\s+0x[0-9a-f]+\s+(0x[0-9a-f]+)',full,re.M)
    if not m: raise SystemExit('Missing map section '+section)
    sizes[section]=int(m[1],16)
if sizes['text']>65536 or sizes['fartext']>65536 or sizes['data']+sizes['bss']>65536:
    raise SystemExit('Kernel exceeds 16-bit segment limits: '+repr(sizes))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
manifest={'elks_commit': subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),
          'segments': sizes,'sha256':{p.name:sha(p) for p in out.iterdir() if p.is_file() and p.name!='manifest.json'},
          'integration_diff_sha256':hashlib.sha256(subprocess.check_output(['git','-C',str(root),'diff'])).hexdigest(),
          'source_sha256':{str(p.relative_to(root)):sha(p) for p in
              (root/'elks/fs/picofs.c',root/'elks/fs/picofs-write.inc',root/'elks/fs/picofs-lfn.inc',root/'elks/fs/picofs-create.inc',root/'elks/arch/i86/drivers/char/mouse-picomem.c',root/'elks/arch/i86/drivers/char/pmmouse.h',root/'elkscmd/sys_utils/mount.c')}}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
