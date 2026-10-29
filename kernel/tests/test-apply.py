#!/usr/bin/env python3
"""Verify fresh application and repeated application in a disposable checkout."""
from pathlib import Path
import hashlib, subprocess, sys, tempfile
kit=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='pm-apply-') as d:
    root=Path(d)/'elks'
    subprocess.run(['git','clone','-q','--shared','--no-checkout',sys.argv[1],str(root)],check=True)
    subprocess.run(['git','-C',str(root),'checkout','-q','--detach','e7a56a4334e0ef9a2261d1c2de54db31ba40fdf0'],check=True)
    def apply():subprocess.run([sys.executable,str(kit/'apply.py'),str(root)],check=True)
    def hashes():return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for base in ('elks','elkscmd') for p in (root/base).rglob('*') if p.is_file()}
    apply();first=hashes();apply();assert first==hashes(),'Repeated apply changed source'
    conf=(root/'elks/fs/config.in').read_text()
    for key in ('CONFIG_FS_PICOMEM_WRITE','CONFIG_FS_PICOMEM_LFN','CONFIG_FS_PICOMEM_LFN_CREATE','CONFIG_FS_PICOMEM_LFN_RENAME','CONFIG_FS_PICOMEM_DIAG'):
        assert conf.count(key+' n')==1,key
    assert (root/'elks/include/linuxmt/fs.h').read_text().count('#define FST_PICOFS')==1
print('Integration: fresh checkout and repeat apply passed; one declaration per option')
