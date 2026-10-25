"""Exercise physical-address decoding and refusal paths on synthetic memory."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent


def memory(segment=0xD000, irq=5):
    data = bytearray(1024 * 1024)
    rom = segment * 16
    data[rom:rom + 12] = b'\x55\xaa\x20\xe9\x00\x00PMBIOS'
    label = b'PicoMEM BIOS (Date 2026-06-16)'
    data[rom + 12:rom + 12 + len(label)] = label
    base = rom + 0x4000
    data[base] = 0x12
    data[base + 4] = 8
    data[base + 11] = irq
    cfg = base + 82
    data[cfg + 32:cfg + 34] = (128).to_bytes(2, 'little')
    data[cfg + 34:cfg + 45] = b'ELKSBOOTIMG'
    data[cfg + 98] = 0x80
    data[cfg + 138:cfg + 138 + 32] = bytes([32]) * 32
    data[cfg + 204:cfg + 206] = bytes([1, 0])
    data[cfg + 215] = irq
    data[cfg + 216:cfg + 218] = (0x300).to_bytes(2, 'little')
    return data


class TestPmcfg(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.dir = pathlib.Path(cls.temp.name)
        cls.exe = cls.dir / 'pmcfg'
        subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                        str(ROOT / 'pmcfg.c'), '-o', str(cls.exe)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_image(self, data, *args):
        path = self.dir / 'memory.bin'
        path.write_bytes(data)
        result = subprocess.run([str(self.exe), '-f', str(path), *args],
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(path.read_bytes(), data, 'input memory was modified')
        return result

    def test_decode(self):
        r = self.run_image(memory())
        self.assertEqual(r.returncode, 0, r.stderr)
        for expected in ['BIOS 2026-06-16', 'HW IRQ: 5',
                         'ne0=5,0x300,,0x92', 'ELKSBOOTIMG',
                         'SRAM on  PSRAM off']:
            self.assertIn(expected, r.stdout)
        self.assertLessEqual(len(r.stdout.splitlines()), 23)
        self.assertLessEqual(max(map(len, r.stdout.splitlines())), 79)

    def test_relocated_rom(self):
        r = self.run_image(memory(0xC800), '-s', 'C800')
        self.assertEqual(r.returncode, 0)
        self.assertIn('ROM C800:0000', r.stdout)

    def test_absent(self):
        self.assertNotEqual(self.run_image(bytearray(1024*1024)).returncode, 0)

    def test_wrong_signature(self):
        data = memory()
        data[0xD0006] = ord('X')
        self.assertNotEqual(self.run_image(data).returncode, 0)

    def test_wrong_shared_id(self):
        data = memory()
        data[0xD4000] = 0
        r = self.run_image(data)
        self.assertNotEqual(r.returncode, 0)
        self.assertNotIn('bootopts candidate', r.stdout)

    def test_truncated(self):
        self.assertNotEqual(self.run_image(memory()[:0xD4100]).returncode, 0)

    def test_multiple_roms_require_selection(self):
        data = memory()
        data[0xC8000:0xC800C] = data[0xD0000:0xD000C]
        self.assertNotEqual(self.run_image(data).returncode, 0)
        self.assertEqual(self.run_image(data, '-s', 'D000').returncode, 0)

    def test_invalid_xt_irq(self):
        r = self.run_image(memory(irq=12))
        self.assertEqual(r.returncode, 0)
        self.assertNotIn('bootopts candidate', r.stdout)

    def test_distinct_hardware_irq(self):
        data = memory(irq=3)
        data[0xD400B] = 7
        r = self.run_image(data)
        self.assertEqual(r.returncode, 0)
        self.assertIn('HW IRQ: 7', r.stdout)
        self.assertIn('NE2000 and BIOS HW IRQ differ', r.stdout)
        self.assertLessEqual(len(r.stdout.splitlines()), 23)

    def test_segment_bounds(self):
        for arg in ['FFFF', 'D001', '-1', 'D000junk', '10000']:
            self.assertEqual(self.run_image(memory(), '-s', arg).returncode, 2)

    def test_details_common_fields(self):
        data = memory()
        b, c = 0xD4000, 0xD4052
        data[b + 2:b + 4] = bytes([9, 4])
        data[b + 17:b + 23] = bytes([0, 1, 128, 2, 2, 2])
        data[c + 102:c + 104] = (32).to_bytes(2, 'little')
        data[c + 104:c + 112] = b'NETBOOT\0'
        data[c + 134] = 0xC8
        data[c + 136] = 2
        for off, val in [(209, 1), (211, 24), (212, 100), (220, 3),
                         (221, 2), (222, 1), (223, 7), (224, 1),
                         (229, 1), (236, 5), (240, 2), (241, 1)]:
            data[c + off] = val
        for off, val in [(230, 0x2C0), (232, 0x220), (234, 0x240), (238, 0x210)]:
            data[c + off:c + off + 2] = val.to_bytes(2, 'little')
        r = self.run_image(data, '--details')
        self.assertEqual(r.returncode, 0, r.stderr)
        for expected in ['BIOS starts: 9  Bootstraps: 4', 'HDD 0 -> 2  FDD 1 -> 2',
                         'BIOS PC memory: 640 KB', 'NETBOOT', 'size 32 KB',
                         'segment C800:0000', 'BIOS SRAM limit: 3 x 16 KB',
                         'BIOS ROM limit: 2 x 16 KB', 'Setup before boot: on',
                         'BIOS boot flag: 7', 'SD clock: 24 MHz  RAM clock: 100 MHz',
                         'RTC: on', 'Menu colour profile: 2', 'Rendering: on',
                         'AdLib: on', 'SoundBlaster IRQ: 5',
                         'Tandy        on   configured port 0x02C0',
                         'CMS          on   configured port 0x0220',
                         'SoundBlaster on   configured port 0x0240',
                         'Mindscape    on   configured port 0x0210']:
            self.assertIn(expected, r.stdout)
        self.assertLessEqual(max(map(len, r.stdout.splitlines())), 79)

    def test_details_extended_names_and_all_directories(self):
        data = memory()
        c = 0xD4052
        data[0xD401B] = 11
        data[c + 244:c + 246] = (1234).to_bytes(2, 'little')
        # Full 14-byte name crosses the old 256-byte boundary.
        data[c + 246:c + 260] = b'CDROM-12345678'
        for i in range(6):
            path = ('/image/path-%d' % i).encode()
            data[c + 260 + 64*i:c + 260 + 64*i + len(path)] = path
        r = self.run_image(data, '--details', '-v')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('CD-ROM: CDROM-12345678  stored size word: 1234', r.stdout)
        for i in range(6):
            self.assertIn('/image/path-%d' % i, r.stdout)
        self.assertIn('Raw profile configuration (768 bytes)', r.stdout)
        self.assertIn('\n2F0:', r.stdout)
        self.assertNotIn('\n300:', r.stdout)

    def test_details_unknown_profile_needs_only_prefix(self):
        data = memory()[:0xD4052 + 256]
        r = self.run_image(data, '--details', '-v')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('Extended fields unavailable', r.stdout)
        self.assertIn('Raw common configuration prefix (256 bytes)', r.stdout)
        self.assertNotIn('\n100:', r.stdout)
        self.assertNotIn('CD-ROM:', r.stdout)

    def test_details_profile_requires_date_and_healthy_init(self):
        for off, val in [(0xD0020, ord('9')), (0xD4005, 0xFD), (0xD4007, 0xFE)]:
            data = memory()[:0xD4052 + 256]
            data[0xD401B] = 11
            data[off] = val
            r = self.run_image(data, '--details')
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn('Extended fields unavailable', r.stdout)

    def test_details_truncated_known_extension_refuses_output(self):
        data = memory()[:0xD4052 + 767]
        data[0xD401B] = 11
        r = self.run_image(data, '--details')
        self.assertNotEqual(r.returncode, 0)
        self.assertEqual(r.stdout, '')
        self.assertIn('cannot read full configuration', r.stderr)

    def test_default_verbose_retains_prefix_even_on_known_board(self):
        data = memory()[:0xD4052 + 256]
        data[0xD401B] = 11
        r = self.run_image(data, '-v')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('Raw common configuration prefix (256 bytes)', r.stdout)
        self.assertNotIn('\n100:', r.stdout)

    def test_details_sanitizes_and_bounds_all_names(self):
        data = memory()
        c = 0xD4052
        data[0xD401B] = 11
        data[c + 104:c + 118] = b'\x1b[2J\x80ROM      '
        data[c + 246:c + 260] = b'X' * 14
        data[c + 260:c + 324] = b'\x1b[2J' + b'P' * 60
        data[c + 324:c + 388] = b'Q' * 64
        # Do not wander into the command parameter area after the config.
        data[c + 768:c + 774] = b'SECRET'
        r = self.run_image(data, '--details')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('?[2J?ROM', r.stdout)
        self.assertIn('CD-ROM: ' + 'X' * 14, r.stdout)
        self.assertIn('    ?[2J' + 'P' * 60 + '\n', r.stdout)
        self.assertIn('    ' + 'Q' * 64 + '\n', r.stdout)
        self.assertNotIn('SECRET', r.stdout)
        self.assertTrue(all(ch == '\n' or 32 <= ord(ch) < 127 for ch in r.stdout))
        self.assertLessEqual(max(map(len, r.stdout.splitlines())), 79)

    def test_details_does_not_claim_live_devices(self):
        r = self.run_image(memory(), '--details')
        self.assertEqual(r.returncode, 0, r.stderr)
        for expected in ['not live ELKS driver status', 'not proof of activation',
                         'not an audio probe', 'unused; not a device inventory',
                         'no blocks assigned', 'SoundBlaster off']:
            self.assertIn(expected, r.stdout)

    def test_details_ids_and_revision(self):
        r = self.run_image(memory(), '--details')
        self.assertIn('Board ID: 0  Pico ID: 0', r.stdout)
        self.assertIn('Firmware revision: not reported', r.stdout)
        self.assertNotIn('prototype', r.stdout)
        data = memory()
        data[0xD401C] = 2
        data[0xD401E:0xD4020] = bytes([0x34, 0x12])
        r = self.run_image(data, '--details')
        self.assertIn('Pico ID: 2', r.stdout)
        self.assertIn('Firmware revision: 0x1234', r.stdout)

    def test_details_can_be_first_argument(self):
        path = self.dir / 'first.bin'
        path.write_bytes(memory())
        r = subprocess.run([str(self.exe), '--details', '-f', str(path)],
                           capture_output=True, text=True, timeout=5)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('Additional configured settings:', r.stdout)

    def test_unknown_option_and_help(self):
        self.assertEqual(self.run_image(memory(), '--details', '--bogus').returncode, 2)
        r = subprocess.run([str(self.exe), '--help'], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0)
        self.assertIn('--details', r.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=2)
