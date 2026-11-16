#!/usr/bin/env python3
"""Reconstruct pinned sources from Git objects, never local working-tree contents."""
import argparse,hashlib,io,json,os,subprocess,tarfile
from pathlib import Path
p=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--destination',type=Path,required=True);ap.add_argument('--repositories',type=Path);ap.add_argument('--cache',type=Path,default=p/'git-cache');args=ap.parse_args()
lock=json.loads((p/'dependencies.lock.json').read_text());repos=json.loads(args.repositories.read_text()) if args.repositories else {};dest=args.destination.resolve();dest.mkdir(parents=True,exist_ok=False)
# Fail on case-insensitive media rather than silently merge upstream directories.
probe=dest/'CaseProbe';probe.write_text('case');assert not (dest/'caseprobe').exists(),'Use a case-sensitive build filesystem';probe.unlink()
report={'components':{},'case_mapping':[],'patches':[]}
def run(*a,**kw):return subprocess.check_output(a,**kw)
def sha(b):return hashlib.sha256(b).hexdigest()
for name,c in lock['components'].items():
 if name in repos:repo=Path(repos[name])
 else:
  repo=args.cache/name
  if not (repo/'HEAD').exists():
   repo.mkdir(parents=True,exist_ok=True);run('git','init','--bare',str(repo));run('git','-C',str(repo),'fetch','--depth=1',c['url'],c['commit'])
 assert run('git','-C',str(repo),'rev-parse',c['commit']+'^{commit}').decode().strip()==c['commit']
 data=run('git','-C',str(repo),'archive','--format=tar',c['commit']);target=dest/c['destination'];target.mkdir(parents=True,exist_ok=True)
 with tarfile.open(fileobj=io.BytesIO(data)) as t:
  for member in t:
   rel=Path(member.name);assert not rel.is_absolute() and '..' not in rel.parts
   f=target/rel
   if member.isdir():f.mkdir(parents=True,exist_ok=True)
   elif member.isfile():
    f.parent.mkdir(parents=True,exist_ok=True);b=t.extractfile(member).read()
    if f.exists():assert f.read_bytes()==b,'Unequal archive collision '+str(f)
    else:f.write_bytes(b);f.chmod(member.mode)
   elif member.issym():
    assert not Path(member.linkname).is_absolute();f.parent.mkdir(parents=True,exist_ok=True)
    assert (f.parent/member.linkname).resolve().is_relative_to(dest),'Escaping symlink'
    if not f.is_symlink():f.symlink_to(member.linkname)
   else:raise RuntimeError('Unsupported archive entry '+member.name)
 report['components'][name]={'commit':c['commit'],'archive_sha256':sha(data),'url':c['url']}
 print('Reconstructed',name,flush=True)
for entry in lock['case_mapping']:
 src=dest/'firmware'/entry['source'];target=dest/'firmware'/entry['destination'];b=src.read_bytes();assert sha(b)==entry['sha256']
 assert hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()==entry['git_blob']
 if target.exists():assert target.read_bytes()==b,'Unequal case-map collision'
 else:target.write_bytes(b);target.chmod(src.stat().st_mode & 0o777)
 report['case_mapping'].append(entry)
for patch in lock['patches']:
 f=p/patch['path'];assert sha(f.read_bytes())==patch['sha256'];cwd=dest/lock['components'][patch['component']]['destination'];run('git','apply','--check',str(f),cwd=cwd);run('git','apply',str(f),cwd=cwd);report['patches'].append(patch)
for f in dest.rglob('*'):
 if f.is_symlink():assert f.resolve().is_relative_to(dest),'External source symlink'
(dest/'reconstruction.json').write_text(json.dumps(report,indent=2)+'\n');print('All sources reconstructed and patches applied')
