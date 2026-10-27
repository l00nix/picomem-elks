#!/usr/bin/env python3
"""Build over configured SSH; keys/agent/config supply credentials, never this repo.
Usage: remote-build.py HOST ABSOLUTE_WORKDIR ABSOLUTE_CROSS_DIR [LOCAL_OUTPUT] [--lfn]
"""
from pathlib import Path
import hashlib, json, shlex, subprocess, sys, tarfile, tempfile
lfn = '--lfn' in sys.argv
if lfn: sys.argv.remove('--lfn')
if len(sys.argv) not in (4,5): raise SystemExit(__doc__)
host, work, cross = sys.argv[1:4]
if host.startswith('-') or any(c.isspace() for c in host): raise SystemExit('Invalid SSH host')
if not work.startswith('/') or not cross.startswith('/') or work=='/': raise SystemExit('Use absolute dedicated paths')
kit=Path(__file__).resolve().parent
out=Path(sys.argv[4] if len(sys.argv)==5 else kit/'build').resolve()
def remote(script):
    subprocess.run(['ssh',host,'bash -se'],input=script,text=True,check=True)
w,c=map(shlex.quote,(work,cross))
remote(f'''if test -e {w} && ! test -f {w}/.picomem-build; then
 echo 'Refusing an existing directory without .picomem-build marker' >&2; exit 1
fi
mkdir -p {w}/src
touch {w}/.picomem-build
test -x {c}/bin/ia16-elf-gcc
''')
with tempfile.TemporaryDirectory() as tmp:
    archive=Path(tmp)/'source.tgz'
    with tarfile.open(archive,'w:gz') as tar:
        for folder in ('kernel','mouse'):
            for p in sorted((kit.parent/folder).rglob('*')):
                relative=p.relative_to(kit.parent)
                if p.is_file() and 'build' not in relative.parts and '__pycache__' not in relative.parts:
                    tar.add(p,arcname=str(relative))
    # SSH stdin transport avoids scp's remote-path interpretation differences.
    with archive.open('rb') as f:
        subprocess.run(['ssh',host,'cat > '+w+'/source.tgz'],stdin=f,check=True)
    remote(f'''tar -xzf {w}/source.tgz -C {w}/src
if ! test -d {w}/elks; then
 git clone https://github.com/ghaerr/elks.git {w}/elks
 git -C {w}/elks checkout --detach e7a56a4334e0ef9a2261d1c2de54db31ba40fdf0
fi
KERNEL_CONFIG={w}/src/kernel/{'ppc512-lfn.config' if lfn else 'ppc512.config'} ELKS_TREE={w}/elks ELKS_TOOLCHAIN={c} bash {w}/src/kernel/build.sh > {w}/build.log 2>&1 || {{ tail -60 {w}/build.log; exit 1; }}
cat {w}/src/kernel/build/manifest.json
''')
    out.mkdir(parents=True,exist_ok=True)
    for name in ('linux','kernel.config','manifest.json','system.map','pmmount','pmmouse','pmfstest','mount','pmrwtest'):
        with (out/name).open('wb') as f:
            subprocess.run(['ssh',host,'cat '+w+'/src/kernel/build/'+name],stdout=f,check=True)
    with (out/'build.log').open('wb') as f:
        subprocess.run(['ssh',host,'cat '+w+'/build.log'],stdout=f,check=True)
manifest=json.loads((out/'manifest.json').read_text())
for name, expected in manifest['sha256'].items():
    if hashlib.sha256((out/name).read_bytes()).hexdigest()!=expected:
        raise SystemExit('Download checksum mismatch: '+name)
print('Build downloaded to',out,'— no hardware installation or reboot performed.')
