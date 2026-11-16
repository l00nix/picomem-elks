from pathlib import Path
import hashlib,json,subprocess,sys
p=Path(__file__).resolve().parent
base=p.parent/'test-inputs'
src=Path(sys.argv[1]).resolve()/'src/pm_usb_ssd.cpp'
s=src.read_text();(p/'production-body.inc').write_text('\n'.join(x for x in s.splitlines() if not x.lstrip().startswith('#include'))+'\n')
t=(base/'cache-test.cpp').read_text()
t=t.replace('static bool disconnect_during_pio;', 'static bool disconnect_during_pio; static unsigned disconnect_pio_call;')
t=t.replace('if(disconnect_during_pio)tuh_msc_umount_cb(1);','if(disconnect_during_pio || (disconnect_pio_call && pio_calls==disconnect_pio_call))tuh_msc_umount_cb(1);')
t=t.replace('disconnect_during_pio=false;', 'disconnect_during_pio=false;disconnect_pio_call=0;')
anchor=' printf("\\n]\\n");'
assert anchor in t
t=t.replace(anchor,''' mounted();disconnect_pio_call=2;check("QA_disconnect_during_512_output",!pm_usb_ssd_command(1)&&read_calls==1&&pio_calls==2&&captured.size()==512&&fault&&bound&&fenced());
 reset();tuh_msc_mount_cb(1);disconnect_pio_call=1;check("QA_disconnect_during_first_claim_output",!pm_usb_ssd_command(4)&&pio_calls==1&&fault&&bound&&fenced());
'''+anchor)
(p/'test-output-handoff.cpp').write_text(t)
cmd=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test-output-handoff.cpp'),'-o',str(p/'test-output-handoff')]
r=subprocess.run(cmd,capture_output=True,text=True);(p/'output-build.log').write_text(r.stdout+r.stderr);r.check_returncode()
r=subprocess.run([str(p/'test-output-handoff')],capture_output=True,text=True);(p/'output-observed.json').write_text(r.stdout);(p/'output-run.log').write_text(r.stderr);r.check_returncode()
a=json.loads(r.stdout);result=dict(source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),cases=len(a),passed=all(x['pass'] for x in a),added_cases=a[-2:],base_cases=574,scope='Actual bridge source, USB/PIO mocked; adds precise output-phase disconnect injections to parent harness.')
(p/'output-result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
