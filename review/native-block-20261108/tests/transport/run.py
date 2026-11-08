from pathlib import Path
import os,sys,shutil,tempfile,subprocess
p=Path(__file__).resolve().parent
firmware=Path(sys.argv[1]).resolve(); tinyusb=Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 for f in p.iterdir():
  if f.suffix in ('.py','.c','.h') and f.name!='run.py':shutil.copyfile(f,d/f.name)
 for n in ('writer.c','writer.h'):shutil.copyfile(firmware/'src/rawblock/rw'/n,d/n)
 env=dict(os.environ,REVIEW_FIRMWARE=str(firmware),REVIEW_TINYUSB=str(tinyusb))
 subprocess.run([sys.executable,str(d/'test-write.py')],env=env,check=True)
