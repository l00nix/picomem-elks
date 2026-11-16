#!/usr/bin/env python3
import argparse,os,subprocess,json,hashlib
from pathlib import Path
p=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);ap.add_argument('--toolchain',type=Path,required=True);a=ap.parse_args();src=a.source.resolve();build=src.parent/'build';build.mkdir(exist_ok=True)
env=os.environ.copy();env['PATH']=str(a.toolchain.resolve())+os.pathsep+env['PATH'];env['SOURCE_DATE_EPOCH']='1794571200';env['TZ']='UTC'
report={'source_date_epoch':env['SOURCE_DATE_EPOCH'],'source':str(src),'case_sensitive':not (src/'Firmware').exists(),'tools':{},'builds':{}}
for name,cmd in [('compiler',['arm-none-eabi-gcc','--version']),('cmake',['cmake','--version']),('make',['make','--version'])]:report['tools'][name]=subprocess.check_output(cmd,env=env).decode().splitlines()[0]
def run(label,cmd):
 with (p/(label+'.log')).open('w') as f:subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,env=env,check=True)
 print('PASS',label,flush=True)
run('picotool-configure',['cmake','-S',str(src/'picotool'),'-B',str(build/'picotool'),'-DPICO_SDK_PATH='+str(src/'sdk'),'-DPICOTOOL_NO_LIBUSB=1','-DPICOTOOL_FLAT_INSTALL=1','-DCMAKE_INSTALL_PREFIX='+str(build/'tools')])
run('picotool-build',['cmake','--build',str(build/'picotool'),'-j4'])
run('picotool-install',['cmake','--install',str(build/'picotool')])
for mode in ['off','on']:
 b=build/mode
 run(mode+'-configure',['cmake','-S',str(src/'firmware/src'),'-B',str(b),'-DPICO_BOARD=pimoroni_pico_plus2_rp2350','-DPICO_PLATFORM=rp2350-arm-s','-DPICO_SDK_PATH='+str(src/'sdk'),'-Dpicotool_DIR='+str(build/'tools'),'-DPM_USB_SSD='+mode.upper(),'-DFETCHCONTENT_FULLY_DISCONNECTED=ON'])
 run(mode+'-build',['cmake','--build',str(b),'-j4'])
 report['builds'][mode]={}
 for name in ['picomem.bin','picomem.uf2','picomem.elf']:
  data=(b/name).read_bytes();report['builds'][mode][name]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
 (p/'build-results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
