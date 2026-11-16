from pathlib import Path
import subprocess,sys
p=Path(__file__).resolve().parent
source=str(Path(sys.argv[1]).resolve())
for script in ['kernel-tests.py','qa/test-geometry.py','qa/backend/run.py']:
 subprocess.run([sys.executable,str(p/script),source],check=True)
