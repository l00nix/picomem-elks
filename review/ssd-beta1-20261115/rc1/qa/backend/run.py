#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,datetime,sys
P=Path(__file__).resolve().parent
src=Path(sys.argv[1]).resolve()/'elks/arch/i86/drivers/block/ssd-picomem.c'
inputs=[src,src.with_name('ssd.h'),P/'acceptance.c',P/'include/mock.h',Path(__file__)]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={str(p):sha(p) for p in inputs}
cmd=['clang','-DPM_BACKEND_SOURCE=\"'+str(src)+'\"','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-sanitize-recover=all','-O1','-g','-I'+str(P/'include'),str(P/'acceptance.c'),'-o',str(P/'acceptance')]
c=subprocess.run(cmd,text=True,capture_output=True)
r=subprocess.run([str(P/'acceptance')],text=True,capture_output=True,timeout=30) if c.returncode==0 else None
(P/'compile.log').write_text(c.stdout+c.stderr)
(P/'run.log').write_text((r.stdout+r.stderr) if r else 'not run\n')
result=dict(complete=True,passed=bool(r and r.returncode==0),created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_sha256=before,input_unchanged=before=={str(p):sha(p) for p in inputs},compiler_command=cmd,compile_exit=c.returncode,run_exit=r.returncode if r else None,case_count=sum(x.startswith('PASS ') for x in r.stdout.splitlines()) if r else 0,summary=r.stdout.splitlines()[-1] if r and r.stdout else None,provenance='Adapted retained read-only mock/harness in a separate qa/backend directory. Includes exact current target-build ssd-picomem.c directly, with no source rewriting. sector_t is uint32_t; host unsigned/unsigned long width differs from16-bit ELKS. Port/IRQ/segmented-copy effects are modeled; no hardware or target timing claims.',limitations=['Mock proves transaction sequence and polling bounds, not actual device removal, USB persistence or real IRQ timing.','Source-level no-retry latch does not repair inherited VFS buffered-write error reporting.'])
result['coverage']=['Read/write byte-exact success and LBA little-endian encoding','One516-byte write input phase; exact counts and no premature success','Wrong stages and both count bytes at applicable init/read/write handshakes','Bounded300000 polls for pending/wrong stage and completed-error clear timeout','Own completed status2/3 cleared once; preexisting status1/2/3/ff never reset','Read caller buffer unchanged on every read failure including final status failure after512 bytes','Failed read/write latch prevents subsequent port I/O and copies','Out-of-range capacity andUINT32_MAX rejected with zero port I/O or copies','Missing counter, missing/busy port, malformed PM magic/sector size/zero capacity','Initialization first/final phase faults','Entry IRQ state restored for both enabled and already-disabled controls','ioctl unsupported returnsEINVAL']
(P/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ['passed','input_unchanged','compile_exit','run_exit','case_count','summary']},indent=2))
if c.returncode:print(c.stderr)
if r and r.returncode:print(r.stdout+r.stderr)
raise SystemExit(0 if result['passed'] and result['input_unchanged'] else 1)
