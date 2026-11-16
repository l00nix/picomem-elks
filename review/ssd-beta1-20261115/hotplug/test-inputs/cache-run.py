from pathlib import Path
import hashlib,subprocess,json
p=Path(__file__).resolve().parent
src=p.parent/'firmware/src/pm_usb_ssd.cpp'
s=src.read_text()
(p/'production-body.inc').write_text('\n'.join(x for x in s.splitlines() if not x.lstrip().startswith('#include'))+'\n')
cmd=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')]
r=subprocess.run(cmd,capture_output=True,text=True);(p/'compile.log').write_text(r.stdout+r.stderr);r.check_returncode()
r=subprocess.run([str(p/'test')],capture_output=True,text=True);(p/'observed.json').write_text(r.stdout);(p/'run.log').write_text(r.stderr)
print(r.stderr,end='');r.check_returncode()
v=json.loads(r.stdout)
(p/'result.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),cases=len(v),passed=all(x['pass'] for x in v),sanitizers=['address','undefined'],actual_source_extracted=True,usb_and_PIO_mocked=True,hardware_access_by_QA=False,retained_transport_cache_cases=532,new_ownership_cases=len(v)-532),indent=2)+'\n')
