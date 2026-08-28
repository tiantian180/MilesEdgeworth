"""Portable skin-file tests with temporary synthetic PNG/GIF fixtures."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from PIL import Image, ImageDraw

import check_skin

CHECKER = Path(__file__).with_name('check_skin.py')


class SkinFileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='pet-skin-files-')
        self.root = Path(self.temp.name) / 'skin'
        (self.root / 'assets/images').mkdir(parents=True)
        (self.root / 'assets/gifs').mkdir()
        frames = []
        for index in range(3):
            frame = Image.new('RGBA', (64, 64))
            ImageDraw.Draw(frame).rectangle((16 + index * 4, 16, 35 + index * 4, 43), fill='white')
            frames.append(frame)
        frames[0].save(self.root / 'assets/images/thumbnail.png')
        frames[0].save(self.root / 'assets/gifs/idle.gif', save_all=True,
                       append_images=frames[1:], duration=[100, 120, 140], loop=0, disposal=2)
        descriptor = {'id': 'sample-pet', 'manifest': 'manifest.json',
                      'thumbnail': 'file:assets/images/thumbnail.png'}
        manifest = {'id': 'sample-pet', 'actions': {
            'idle': {'asset': 'file:assets/gifs/idle.gif'}}}
        for name, value in (('skin.json', descriptor), ('manifest.json', manifest)):
            (self.root / name).write_text(json.dumps(value), encoding='utf-8')
        self.options = argparse.Namespace(min_poses=3, padding=8, canvas=[64, 64])

    def tearDown(self):
        self.temp.cleanup()

    def issues(self):
        report = check_skin.check_skin(self.root, self.options)
        return report['errors'] + [e for a in report['assets'] for e in a['errors']]

    def edit_json(self, name, callback):
        path = self.root / name
        data = json.loads(path.read_text(encoding='utf-8-sig'))
        callback(data)
        path.write_text(json.dumps(data), encoding='utf-8')

    def test_valid_skin(self):
        self.assertEqual(self.issues(), [])

    def test_cli_success(self):
        process = subprocess.run([sys.executable, '-B', str(CHECKER), str(self.root)],
                                 capture_output=True, text=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertTrue(json.loads(process.stdout)['passed'])

    def test_static_only_package(self):
        (self.root / 'assets/gifs/idle.gif').unlink()
        self.edit_json('manifest.json', lambda d: d.update(actions={
            'idle': {'asset': 'file:assets/images/thumbnail.png'}}))
        self.assertEqual(self.issues(), [])

    def test_missing_thumbnail(self):
        self.edit_json('skin.json', lambda d: d.update(thumbnail='file:assets/images/missing.png'))
        self.assertTrue(any('missing file' in e for e in self.issues()))

    def test_escape_root(self):
        self.edit_json('skin.json', lambda d: d.update(thumbnail='file:../outside.png'))
        self.assertTrue(any('escapes skin root' in e for e in self.issues()))

    def test_percent_encoded_escape(self):
        self.edit_json('skin.json', lambda d: d.update(thumbnail='file:%2e%2e/outside.png'))
        self.assertTrue(any('escapes skin root' in e for e in self.issues()))

    def test_bad_manifest(self):
        (self.root / 'manifest.json').write_text('{invalid', encoding='utf-8')
        self.assertTrue(any('invalid skin metadata' in e for e in self.issues()))

    def test_nonstring_manifest_is_reported_not_crashed(self):
        self.edit_json('skin.json', lambda d: d.update(manifest=123))
        self.assertTrue(any('invalid skin metadata' in e for e in self.issues()))

    def test_missing_ids_are_not_a_valid_match(self):
        self.edit_json('skin.json', lambda d: d.pop('id'))
        self.edit_json('manifest.json', lambda d: d.pop('id'))
        self.assertTrue(any('id' in e for e in self.issues()))

    def test_invalid_equal_ids_are_not_a_valid_match(self):
        for value in ('', '   ', 123, None):
            with self.subTest(value=value):
                self.edit_json('skin.json', lambda d: d.update(id=value))
                self.edit_json('manifest.json', lambda d: d.update(id=value))
                self.assertTrue(any('id must be a nonempty string' in e for e in self.issues()))

    def test_mixed_directory(self):
        source = next((self.root / 'assets/gifs').glob('*.gif'))
        shutil.copy2(source, self.root / 'assets' / source.name)
        self.assertTrue(any('wrong folder' in e for e in self.issues()))

    def test_referenced_image_outside_assets(self):
        source = next((self.root / 'assets/images').glob('*.png'))
        shutil.copy2(source, self.root / 'thumbnail.png')
        self.edit_json('skin.json', lambda d: d.update(thumbnail='file:thumbnail.png'))
        self.assertTrue(any('wrong folder' in e for e in self.issues()))

    def test_still_renamed_gif(self):
        source = next((self.root / 'assets/images').glob('*.png'))
        shutil.copy2(source, self.root / 'assets/gifs/fake.gif')
        self.assertTrue(any('decoded format is PNG' in e for e in self.issues()))

    def test_wrong_canvas(self):
        self.options.canvas = [32, 32]
        self.assertTrue(any('canvas differs' in e for e in self.issues()))

    def test_failed_root_does_not_skip_other_roots(self):
        process = subprocess.run([sys.executable, str(CHECKER), str(self.root / 'absent'), str(self.root)],
                                 capture_output=True, text=True, timeout=30)
        self.assertEqual(process.returncode, 1)
        self.assertEqual(len(json.loads(process.stdout)['skins']), 2)


if __name__ == '__main__':
    unittest.main()
