from pathlib import Path
import argparse,hashlib,json,subprocess,shutil,re,struct
p=Path(__file__).resolve().parent;ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);ap.add_argument('--toolchain',type=Path,required=True);a=ap.parse_args();src=a.source.resolve();b=src.parent/'build';q=p/'tests';q.mkdir(exist_ok=True)
inputs=p/'test-inputs';report={}
def run(label,cmd):
 r=subprocess.run(cmd,capture_output=True,text=True);(q/(label+'.log')).write_text(r.stdout+r.stderr);r.check_returncode();return r.stdout
for name in ['test-handshake.cpp','wait-ready.inc','pm-case.inc','read-tail.inc']:
 shutil.copyfile(inputs/name,q/name)
 if name.endswith('.inc'):assert (q/name).read_text() in (src/'firmware/src/pm_isa_rp2350_m2_v15.h').read_text()
for mode in [0,1]:
 exe=q/('handshake-'+str(mode));run('compile-'+exe.name,['clang++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-const-variable','-Wno-shift-op-parentheses','-fsanitize=address,undefined','-DPM_USB_SSD='+str(mode),str(q/'test-handshake.cpp'),'-o',str(exe)]);report[exe.name]=run(exe.name,[str(exe)])
for n in ['acceptance.c','tusb_config.h']:shutil.copyfile(inputs/n,q/n)
run('compile-msc',['clang','-std=c99','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,-dead_strip','-I'+str(q),'-I'+str(src/'firmware/src/lib/tinyusb/src'),str(q/'acceptance.c'),'-o',str(q/'msc')]);v=json.loads(run('msc',[str(q/'msc')]));assert all(x['pass'] for x in v);report['msc_cases']=len(v)
for name,oldscript,testfile,source in [('pio',inputs/'pio-run.py',None,src/'firmware/src/isa_devices/dev_picomem_io.cpp'),('cache',inputs/'cache-run.py',inputs/'cache-test.cpp',src/'firmware/src/pm_usb_ssd.cpp')]:
 d=q/name;d.mkdir(exist_ok=True)
 if testfile:shutil.copyfile(testfile,d/'test.cpp')
 s=oldscript.read_text();s=re.sub(r"src=p.parent/[^\n]+",'src=Path('+repr(str(source))+')',s);(d/'run.py').write_text(s);run(name,['python3',str(d/'run.py')]);report[name]=json.loads((d/'result.json').read_text())
old=(inputs/'expected.pio.h').read_bytes();new=(b/'on/isa_iomem_m2_v15.pio.h').read_bytes();assert old==new;report['generated_pio_exact_to_tested']=True
report['images']={}
for mode in ['off','on']:
 elf=b/mode/'picomem.elf';nm=run(mode+'-nm',[str(a.toolchain/'arm-none-eabi-nm'),'-n',str(elf)]);syms={x.split()[2]:int(x.split()[0],16) for x in nm.splitlines() if len(x.split())==3 and x.split()[0]!='U'}
 assert syms['__scratch_x_end__']<=syms['__StackOneBottom'];assert syms['__scratch_y_end__']<=syms['__StackBottom']
 assert ('pm_usb_ssd_command' in nm)==(mode=='on');assert 'retained_crc' not in nm
 data=(b/mode/'picomem.bin').read_bytes();uf=(b/mode/'picomem.uf2').read_bytes();payload=bytearray()
 for o in range(0,len(uf),512):
  h=struct.unpack_from('<8I',uf,o);assert h[:2]==(0x0a324655,0x9e5d5157) and struct.unpack_from('<I',uf,o+508)[0]==0x0ab16f30
  if h[7]==0xe48bff59:assert h[3]==0x10000000+len(payload) and h[4]==256;payload.extend(uf[o+32:o+288])
 assert payload[:len(data)]==data and not any(payload[len(data):]);report['images'][mode]={'uf2_bin_match':True,'stacks_disjoint':True,'bridge_linked':mode=='on'}
(p/'qualification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
