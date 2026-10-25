import pathlib
import subprocess
import tempfile
import unittest
import zlib
from test_pmcfg import memory

ROOT = pathlib.Path(__file__).resolve().parent
BEFORE, AFTER, CRC = 160, 928, 1696


class EditorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.exe = pathlib.Path(cls.build.name) / 'pmcfg'
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                        str(ROOT/'pmcfg.c'),'-o',str(cls.exe)],check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dir = pathlib.Path(self.tmp.name)
        self.image = self.dir/'memory.bin'
        self.data = memory(irq=3)
        self.data[0xd4000+27] = 11
        self.data[0xd4000+12] = 0xa8
        self.image.write_bytes(self.data)
        self.stage = self.dir/'stage.pmc'

    def run_cmd(self,*args,image=True):
        command = [str(self.exe),*map(str,args)]
        if image: command += ['--image',str(self.image)]
        r = subprocess.run(command,capture_output=True,text=True,timeout=10)
        self.assertEqual(self.image.read_bytes(),self.data)
        return r

    def stage_irq(self,irq=5):
        r=self.run_cmd('--stage',self.stage,f'nic-irq={irq}')
        self.assertEqual(r.returncode,0,r.stderr)
        return bytearray(self.stage.read_bytes())

    def reseal(self,b):
        b[CRC:] = zlib.crc32(b[:CRC]).to_bytes(4,'little')
        self.stage.write_bytes(b)

    def test_stage_changes_only_allowed_byte(self):
        b=self.stage_irq()
        self.assertEqual(len(b),1700)
        self.assertEqual(int.from_bytes(b[CRC:],'little'),zlib.crc32(b[:CRC]))
        changed=[i for i in range(768) if b[BEFORE+i]!=b[AFTER+i]]
        self.assertEqual(changed,[215])
        self.assertEqual(b[AFTER+215],5)
        self.assertEqual(self.stage.stat().st_mode & 0o777,0o600)
        r=self.run_cmd('--show',self.stage,image=False)
        self.assertEqual(r.returncode,0)
        self.assertIn('STAGED FOR NEXT BOOT',r.stdout)

    def test_fast_boot(self):
        r=self.run_cmd('--stage',self.stage,'fast-boot=on')
        self.assertEqual(r.returncode,0,r.stderr)
        b=self.stage.read_bytes()
        self.assertEqual(b[AFTER+207],1)
        self.assertEqual(b[AFTER+215],3)

    def test_bad_settings(self):
        for item in ['nic-irq=12','nic-irq=4','nic-irq=3x','ram=off',
                     'fast-boot=yes','nic-port=0x320']:
            r=self.run_cmd('--stage',self.stage,item)
            self.assertNotEqual(r.returncode,0,item)
            self.assertFalse(self.stage.exists())

    def test_irq_not_available(self):
        self.data[0xd400c]=0x88
        self.image.write_bytes(self.data)
        self.assertNotEqual(self.run_cmd('--stage',self.stage,'nic-irq=5').returncode,0)
        self.assertFalse(self.stage.exists())

    def test_duplicate_settings(self):
        r=self.run_cmd('--stage',self.stage,'nic-irq=3','nic-irq=5')
        self.assertNotEqual(r.returncode,0)
        self.assertFalse(self.stage.exists())

    def test_exclusive_stage(self):
        b=self.stage_irq()
        r=self.run_cmd('--stage',self.stage,'fast-boot=on')
        self.assertNotEqual(r.returncode,0)
        self.assertEqual(self.stage.read_bytes(),b)

    def test_corruption_and_wrong_size(self):
        b=self.stage_irq()
        for variant in [b[:-1],b+b'x',b[:200]+bytes([b[200]^1])+b[201:]]:
            self.stage.write_bytes(variant)
            self.assertNotEqual(self.run_cmd('--show',self.stage,image=False).returncode,0)

    def test_resealed_forbidden_edit(self):
        b=self.stage_irq();b[AFTER+138]^=1;self.reseal(b)
        self.assertNotEqual(self.run_cmd('--show',self.stage,image=False).returncode,0)

    def test_resealed_bad_irq(self):
        b=self.stage_irq();b[AFTER+215]=4;self.reseal(b)
        self.assertNotEqual(self.run_cmd('--show',self.stage,image=False).returncode,0)

    def test_unknown_profile(self):
        self.data[0xd4000+27]=1
        self.image.write_bytes(self.data)
        self.assertNotEqual(self.run_cmd('--stage',self.stage,'nic-irq=5').returncode,0)

    def test_unhealthy_sd(self):
        self.data[0xd4005]=0xfd
        self.image.write_bytes(self.data)
        self.assertNotEqual(self.run_cmd('--stage',self.stage,'nic-irq=5').returncode,0)

    def test_wrong_bios_date(self):
        self.data[0xd0000:0xd0080]=self.data[0xd0000:0xd0080].replace(b'2026-06-16',b'2025-06-16')
        self.image.write_bytes(self.data)
        self.assertNotEqual(self.run_cmd('--stage',self.stage,'nic-irq=5').returncode,0)

    def test_restore_stages_original_fields_only(self):
        b=self.stage_irq()
        self.data[0xd4000+82+215]=5
        self.data[0xd4000+82+211]=24  # retain unrelated changes made since backup
        self.image.write_bytes(self.data)
        dest=self.dir/'restore.pmc'
        r=self.run_cmd('--restore',self.stage,'--stage',dest)
        self.assertEqual(r.returncode,0,r.stderr)
        b=dest.read_bytes()
        self.assertEqual(b[AFTER+215],3)
        self.assertEqual(b[BEFORE+215],5)
        self.assertEqual(b[AFTER+211],24)

    def test_fixture_save_prohibited(self):
        self.stage_irq()
        backup=self.dir/'backup.pmc'
        r=self.run_cmd('--save',self.stage,'--backup',backup)
        self.assertNotEqual(r.returncode,0)
        self.assertFalse(backup.exists())


if __name__=='__main__': unittest.main(verbosity=2)
