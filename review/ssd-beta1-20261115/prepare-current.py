#!/usr/bin/env python3
"""Reconstruct the pinned RC1 sources, then apply the two verified hotplug deltas."""
import argparse, hashlib, json, subprocess, sys
from pathlib import Path
p=Path(__file__).resolve().parent
ap=argparse.ArgumentParser()
ap.add_argument('--destination',type=Path,required=True)
ap.add_argument('--repositories',type=Path)
ap.add_argument('--cache',type=Path)
a=ap.parse_args()
dest=a.destination.resolve()
cmd=[sys.executable,str(p/'rc1/prepare.py'),'--destination',str(dest)]
for key in ['repositories','cache']:
 if getattr(a,key):cmd.extend(['--'+key,str(getattr(a,key).resolve())])
subprocess.run(cmd,check=True)
record=json.loads((p/'hotplug/source-delta.json').read_text())
for component,directory in [('elks','elks'),('picomem','firmware')]:
 patch=p/'hotplug/patches'/('hotplug-'+component+'.patch')
 assert hashlib.sha256(patch.read_bytes()).hexdigest()==record['patches'][component]['sha256']
 subprocess.run(['git','apply','--check',str(patch)],cwd=dest/directory,check=True)
 subprocess.run(['git','apply',str(patch)],cwd=dest/directory,check=True)
print('Current hotplug candidate reconstructed; optional fdisk is not applied.')
