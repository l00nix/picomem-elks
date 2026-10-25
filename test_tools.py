"""Verify support record boundaries, private/exclusive writes and aliases."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_pmcfg import memory, ROOT

spec=importlib.util.spec_from_file_location('dump_reader', ROOT/'tools/read-dump.py')
reader=importlib.util.module_from_spec(spec); spec.loader.exec_module(reader)


class ToolTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(); cls.root=Path(cls.temp.name)
        cls.exe=cls.root/'pmconfig'
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',str(ROOT/'pmcfg.c'),'-o',str(cls.exe)],check=True)
        for name in ['pmdump','pmmem','pmstat','pmcfg']: shutil.copyfile(cls.exe,cls.root/name)
        for name in ['pmdump','pmmem','pmstat','pmcfg']: os.chmod(cls.root/name,0o755)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def setUp(self):
        self.data=memory(); self.data[0xD401B]=11; self.data[0xD400C]=0xa8
        self.data[0xD4052+768:0xD4052+778]=b'PRIVATEKEY'
        self.image=self.root/'image'; self.image.write_bytes(self.data)
        self.dest=self.root/self._testMethodName

    def run_tool(self,name,*args):
        r=subprocess.run([str(self.root/name),*map(str,args)],capture_output=True,text=True,timeout=5)
        self.assertEqual(self.image.read_bytes(),self.data)
        return r

    def test_dump_format_and_bounds(self):
        r=self.run_tool('pmdump',self.dest,'-f',self.image)
        self.assertEqual(r.returncode,0,r.stderr)
        d=self.dest.read_bytes(); self.assertEqual(len(d),964)
        self.assertEqual(d[192:960],self.data[0xD4052:0xD4052+768])
        self.assertNotIn(b'PRIVATEKEY',d)
        self.assertEqual(reader.decode(d)['nic_irq'],5)
        self.assertEqual(reader.decode(d)['source'],'fixture')
        self.assertEqual(self.dest.stat().st_mode & 0o777,0o600)

    def test_no_overwrite(self):
        self.dest.write_bytes(b'keep')
        self.assertNotEqual(self.run_tool('pmconfig','--dump',self.dest,'-f',self.image).returncode,0)
        self.assertEqual(self.dest.read_bytes(),b'keep')

    def test_no_symlink_follow(self):
        target=self.root/'preserved'; target.write_bytes(b'keep'); self.dest.symlink_to(target)
        self.assertNotEqual(self.run_tool('pmdump',self.dest,'-f',self.image).returncode,0)
        self.assertEqual(target.read_bytes(),b'keep')

    def test_unknown_profile_no_file(self):
        self.data[0xD401B]=1; self.image.write_bytes(self.data)
        self.assertNotEqual(self.run_tool('pmdump',self.dest,'-f',self.image).returncode,0)
        self.assertFalse(self.dest.exists())

    def test_truncated_extension_no_file(self):
        self.data=self.data[:0xD4052+767]; self.image.write_bytes(self.data)
        self.assertNotEqual(self.run_tool('pmdump',self.dest,'-f',self.image).returncode,0)
        self.assertFalse(self.dest.exists())

    def test_reader_rejects_corruption_and_wrong_length(self):
        self.assertEqual(self.run_tool('pmdump',self.dest,'-f',self.image).returncode,0)
        d=self.dest.read_bytes()
        for bad in [d[:-1],d+b'X',d[:300]+bytes([d[300]^1])+d[301:]]:
            with self.assertRaises(ValueError): reader.decode(bad)

    def test_status_is_passive(self):
        r=self.run_tool('pmstat','-f',self.image)
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertIn('no firmware commands',r.stdout)
        self.assertIn('IRQ 5',r.stdout)
        self.assertNotIn('PRIVATEKEY',r.stdout)

    def test_memory_fixture_refused(self):
        for mode in ['--memory','--probe','--usb','--disks']:
            self.assertEqual(self.run_tool('pmconfig',mode,'-f',self.image).returncode,2)

    def test_live_status_conflicts_and_profile(self):
        for args in [('--usb','--disks'),('--usb','--memory'),('--usb','--dump',self.dest)]:
            self.assertEqual(self.run_tool('pmconfig',*args,'-f',self.image).returncode,2)
        self.assertEqual(self.run_tool('pmstat','--usb','--usb').returncode,2)

    def test_modes_and_aliases(self):
        self.assertEqual(self.run_tool('pmcfg','-f',self.image).returncode,0)
        for name in ['pmdump','pmmem','pmstat']:
            self.assertEqual(self.run_tool(name,'--help').returncode,0)
            self.assertEqual(self.run_tool(name,'--save','anything','--backup','other').returncode,2)
        self.assertEqual(self.run_tool('pmconfig','--status','--dump',self.dest,'-f',self.image).returncode,2)


if __name__=='__main__': unittest.main(verbosity=2)
