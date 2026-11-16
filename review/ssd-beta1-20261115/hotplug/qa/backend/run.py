#!/usr/bin/env python3
"""Compile actual candidate backend against port mocks; no hardware access."""
from pathlib import Path
import hashlib,json,subprocess,datetime,sys
P=Path(__file__).resolve().parent
src=Path(sys.argv[1]).resolve()/'elks/arch/i86/drivers/block/ssd-picomem.c'
old=P.parents[2]/'rc1/qa/backend'
inputs=[src,src.with_name('ssd.h'),Path(sys.argv[2]).resolve()/'src/isa_devices/dev_picomem_io.cpp',Path(sys.argv[2]).resolve()/'src/pm_cmd.cpp',old/'acceptance.c',old/'include/mock.h',P/'acceptance.c',P/'include/mock.h',Path(__file__)]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={str(p):sha(p) for p in inputs}
cmd=['clang','-DPM_BACKEND_SOURCE="'+str(src)+'"','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-sanitize-recover=all','-O1','-g','-I'+str(P/'include'),str(P/'acceptance.c'),'-o',str(P/'acceptance')]
c=subprocess.run(cmd,text=True,capture_output=True)
r=subprocess.run([str(P/'acceptance')],text=True,capture_output=True,timeout=30) if c.returncode==0 else None
(P/'compile.log').write_text(c.stdout+c.stderr);(P/'run.log').write_text(r.stdout+r.stderr if r else 'not run\n')
result=dict(complete=True,passed=bool(r and r.returncode==0),created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_sha256=before,input_unchanged=before=={str(p):sha(p) for p in inputs},compiler_command=cmd,compile_exit=c.returncode,run_exit=r.returncode if r else None,case_count=sum(x.startswith('PASS ') for x in r.stdout.splitlines()) if r else 0,summary=r.stdout.splitlines()[-1] if r and r.stdout else None,hardware_accessed=False,production_source_modified=False,provenance='Adapted RC1 actual-source143-case backend harness in new qa directory. Includes candidate source directly unchanged. Updated geometry operation0 to claim operation4; zero capacity now tested as retryable valid no-media response; added status-only release operation3.',coverage=['Exact read/write success, stage/count errors, finite polling and IRQ restoration retained for both entry IRQ states','Operation4 complete validated no-media returns0 without hard fault, repeated retries, later insertion and new capacity','Operation3 status-only release, finite delayed READY success and subsequent claim/read/write with changed capacity','Release pending/wrong stage/absent/error latch, own completed error clear only, failed clear bounded','Init/read/write/release permanent no-I/O reuse after fault even when late READY and good geometry appear','Out-of-range zero port/copy operations and unchanged read caller on all failures'],limitations=['Port/IRQ/copy models only; not electrical timing or USB insertion/removal qualification.','Generic ssd_open/release media ownership/capacity/cache lifecycle and firmware state machine tested by separate auditors. Static integration check: dev_picomem_io.cpp:158-159 sets CMDINPROGRESS when command accepted; pm_cmd.cpp:529-531 publishes final status after service returns, so status-only release does not retain old READY.','Host unsigned widths differ from16-bit ELKS; sector_t is uint32_t.'])
(P/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result[k] for k in ['passed','input_unchanged','compile_exit','run_exit','case_count','summary']},indent=2))
if c.returncode:print(c.stderr)
if r and r.returncode:print(r.stdout+r.stderr)
raise SystemExit(0 if result['passed'] and result['input_unchanged'] else 1)
