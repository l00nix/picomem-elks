from pathlib import Path
import subprocess,os
p=Path(__file__).resolve().parent
src=Path(os.environ['REVIEW_TINYUSB'])/'src'
# Generate and run the unchanged sector-write-blocked integration cases first.
subprocess.run(['python3',str(p/'test-integrated.py')],check=True)
text=(p/'test-integrated.c').read_text()
assert text.count('\nint main(void)')==1
text=text.replace('\nint main(void)','\nvoid unused_recovery_main(void)')
(p/'test-write.c').write_text(text+'\n'+(p/'write-cases.c').read_text())
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-pthread','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections',('-Wl,-dead_strip' if __import__('sys').platform == 'darwin' else '-Wl,--gc-sections'),'-DPM_RAW_USB_RECOVERED_WRITE=1','-DPM_RAW_USB_CSW_RECOVERY=1','-DPM_RAW_USB_RP2_COMPARE=1','-DPM_RAW_USB_EMPTY_CACHE_WT=1','-DPMW_PINNED_MSC','-DPM_RAW_USB_RO','-DPM_RAW_USB_RW_LAB','-DPMW_MSC_SOURCE="'+str(src.resolve()/'class/msc/msc_host.c')+'"','-I'+str(p),'-I'+str(src),'-I'+str(Path(os.environ['REVIEW_FIRMWARE'])/'tests/rawblock/stubs'),str(p/'test-write.c'),'-o',str(p/'test-write')]
subprocess.run(cmd,check=True)
subprocess.run([str(p/'test-write')],check=True,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
