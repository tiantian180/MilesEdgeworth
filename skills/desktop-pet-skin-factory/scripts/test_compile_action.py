"""Isolated synthetic encoding fixtures, not generated pet artwork or visual approval."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image, ImageDraw, ImageOps

from compile_action import compile_action, indexed_frames, visible_hash


class CompileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='pet-action-test-')
        self.root = Path(self.temp.name)
        (self.root / 'images').mkdir()
        for i in range(3):
            image = Image.new('RGBA', (64, 64))
            draw = ImageDraw.Draw(image)
            draw.rectangle((20, 20, 39, 49), fill=(245, 245, 245, 255))
            # A moving detached prop must survive, as must bright white body pixels.
            draw.rectangle((42 + i * 3, 15, 44 + i * 3, 17), fill=(230, 25 + i * 80, 30, 255))
            image.save(self.root / 'images' / f'{i}.png')
        self.plan = {'actionId': 'fixture', 'canvas': [64, 64], 'targetAnchor': [32, 49],
                     'scale': 1, 'padding': 4, 'alphaThreshold': 100, 'loop': True, 'baselineY': 49,
                     'frames': [{'source': f'images/{i}.png', 'crop': [0, 0, 64, 64], 'anchor': [32, 49],
                                 'contactPoint': [32, 49], 'durationMs': delay}
                                for i, delay in enumerate((140, 80, 160))]}

    def tearDown(self):
        self.temp.cleanup()

    def run_plan(self, name='out'):
        path = self.root / 'plan.json'
        path.write_text(json.dumps(self.plan), encoding='utf-8')
        return compile_action(path, self.root / name)

    def gif_frames(self, name='out'):
        with Image.open(self.root / name / 'gifs' / 'fixture.gif') as gif:
            frames = []
            for i in range(gif.n_frames):
                gif.seek(i)
                frames.append(gif.convert('RGBA'))
            return frames

    def test_roundtrip_white_body_and_detached_prop(self):
        report = self.run_plan()
        self.assertEqual(report['encodedFrames'], 3)
        self.assertEqual(report['durationMs'], 380)
        self.assertEqual(report['status'], 'compiled')
        self.assertIsNone(report['checks']['visual'])
        self.assertIsNone(report['distinctPoses'])
        for i, frame in enumerate(self.gif_frames()):
            self.assertEqual(frame.getpixel((0, 0))[3], 0)
            self.assertEqual(frame.getpixel((30, 30))[3], 255)
            self.assertEqual(frame.getpixel((43 + i * 3, 16))[3], 255)
        self.assertTrue((self.root / 'out/qa/light.png').exists())
        self.assertTrue((self.root / 'out/qa/dark.png').exists())
        self.assertEqual(len(list((self.root / 'out/images').glob('*.png'))), 3)

    def test_one_shot_has_no_loop_extension(self):
        self.plan['loop'] = False
        self.run_plan()
        with Image.open(self.root / 'out/gifs/fixture.gif') as gif:
            self.assertNotIn('loop', gif.info)

    def test_mirror_preserves_time_order(self):
        self.run_plan('right')
        self.plan.update(mirrorX=True, mirrorReason='Synthetic fixture is mirror-safe')
        self.run_plan('left')
        for right, left in zip(self.gif_frames('right'), self.gif_frames('left')):
            self.assertEqual(visible_hash(ImageOps.mirror(right)), visible_hash(left))

    def test_requires_mirror_decision(self):
        self.plan['mirrorX'] = True
        with self.assertRaisesRegex(ValueError, 'mirrorReason'):
            self.run_plan()

    def test_source_bytes_are_preserved_and_rebuild_is_deterministic(self):
        before = {path: path.read_bytes() for path in (self.root / 'images').glob('*.png')}
        first = self.run_plan('one')
        second = self.run_plan('two')
        self.assertEqual(first, second)
        self.assertEqual((self.root / 'one/gifs/fixture.gif').read_bytes(), (self.root / 'two/gifs/fixture.gif').read_bytes())
        self.assertEqual(before, {path: path.read_bytes() for path in before})

    def test_refuse_overwrite(self):
        self.run_plan()
        before = (self.root / 'out/report.json').read_bytes()
        with self.assertRaisesRegex(ValueError, 'already exists'):
            self.run_plan()
        self.assertEqual(before, (self.root / 'out/report.json').read_bytes())

    def test_reject_rgb(self):
        with Image.open(self.root / 'images/0.png') as image:
            rgb = image.convert('RGB')
        rgb.save(self.root / 'images/0.png')
        with self.assertRaisesRegex(ValueError, 'no alpha'):
            self.run_plan()
        self.assertFalse((self.root / 'out').exists())

    def test_reject_solid_fake_alpha(self):
        Image.new('RGBA', (64, 64), 'white').save(self.root / 'images/0.png')
        with self.assertRaisesRegex(ValueError, 'source crop'):
            self.run_plan()

    def test_reject_clipped_source(self):
        self.plan['frames'][0]['crop'] = [20, 10, 40, 50]
        with self.assertRaisesRegex(ValueError, 'source crop'):
            self.run_plan()

    def test_reject_output_clipping(self):
        self.plan['targetAnchor'] = [50, 49]
        with self.assertRaisesRegex(ValueError, 'output clips'):
            self.run_plan()

    def test_reject_baseline_drift(self):
        self.plan['frames'][1]['contactPoint'][1] = 45
        with self.assertRaisesRegex(ValueError, 'baseline drift'):
            self.run_plan()

    def test_flight_keeps_intentional_vertical_motion(self):
        self.plan.pop('baselineY')
        for frame in self.plan['frames']:
            frame.pop('contactPoint')
        image = Image.new('RGBA', (64, 64))
        ImageDraw.Draw(image).rectangle((20, 10, 39, 39), fill='white')
        image.save(self.root / 'images/1.png')
        report = self.run_plan()
        self.assertEqual(report['frames'][0]['outputBounds'][3] - report['frames'][1]['outputBounds'][3], 10)

    def test_distributed_template_preserves_ungrounded_travel(self):
        # Exercise the shipped template, not a separately reconstructed plan.
        template = Path(__file__).resolve().parents[1] / 'assets' / 'action-plan.json'
        self.plan = json.loads(template.read_text(encoding='utf-8'))
        expected = [(180, 200, 280, 280), (212, 176, 312, 256)]
        for item, box in zip(self.plan['frames'], expected):
            frame = Image.new('RGBA', tuple(self.plan['canvas']))
            ImageDraw.Draw(frame).rectangle((box[0], box[1], box[2] - 1, box[3] - 1), fill='white')
            frame.save(self.root / item['source'])
        report = self.run_plan()
        self.assertEqual(report['encodedFrames'], 2)
        self.assertTrue(all(frame['contactPoint'] is None for frame in report['frames']))
        with Image.open(self.root / 'out/gifs/greeting.gif') as gif:
            for index, box in enumerate(expected):
                gif.seek(index)
                self.assertEqual(gif.convert('RGBA').getchannel('A').getbbox(), box)
                with Image.open(self.root / 'out/images' / f'{index:03d}.png') as png:
                    self.assertEqual(png.getchannel('A').getbbox(), box)
        self.assertIsNone(report['checks']['visual'])
        self.assertIsNone(report['checks']['runtime'])

    def test_cell_local_reference_maps_to_absolute_sheet_anchor(self):
        # The second cell is at x=64. Its fixed local anchor is still (32,32),
        # not the moving body's center; output must retain +6,-8 travel.
        sheet = Image.new('RGBA', (128, 64))
        draw = ImageDraw.Draw(sheet)
        draw.rectangle((20, 24, 35, 39), fill='white')
        draw.rectangle((90, 16, 105, 31), fill='white')
        sheet.save(self.root / 'images/sheet.png')
        self.plan.pop('baselineY')
        self.plan['targetAnchor'] = [32, 32]
        self.plan['frames'] = [
            {'source': 'images/sheet.png', 'crop': [0, 0, 64, 64],
             'anchor': [32, 32], 'durationMs': 140},
            {'source': 'images/sheet.png', 'crop': [64, 0, 64, 64],
             'anchor': [96, 32], 'durationMs': 100},
        ]
        report = self.run_plan()
        expected = [(20, 24, 36, 40), (26, 16, 42, 32)]
        self.assertEqual([tuple(frame['outputBounds']) for frame in report['frames']], expected)
        self.assertEqual([frame.getchannel('A').getbbox() for frame in self.gif_frames()], expected)
        # Following the moving body instead would erase the intentional travel.
        self.plan['frames'][0]['anchor'] = [28, 32]
        self.plan['frames'][1]['anchor'] = [98, 24]
        with self.assertRaisesRegex(ValueError, 'all frames are identical'):
            self.run_plan('recentered')

    def test_reject_bad_duration(self):
        self.plan['frames'][0]['durationMs'] = 83
        with self.assertRaisesRegex(ValueError, '10ms'):
            self.run_plan()

    def test_reject_escape(self):
        self.plan['frames'][0]['source'] = '../outside.png'
        with self.assertRaisesRegex(ValueError, 'escapes'):
            self.run_plan()

    def test_reject_empty_frame(self):
        Image.new('RGBA', (64, 64)).save(self.root / 'images/0.png')
        with self.assertRaisesRegex(ValueError, 'empty'):
            self.run_plan()

    def test_reject_identical_frames(self):
        self.plan['frames'] = [copy.deepcopy(self.plan['frames'][0]) for _ in range(3)]
        with self.assertRaisesRegex(ValueError, 'identical'):
            self.run_plan()

    def test_adjacent_holds_must_use_duration(self):
        self.plan['frames'].insert(1, copy.deepcopy(self.plan['frames'][0]))
        with self.assertRaisesRegex(ValueError, 'adjacent frames'):
            self.run_plan()

    def test_alpha_threshold_cannot_erase_frame(self):
        with Image.open(self.root / 'images/1.png') as image:
            faint = image.convert('RGBA')
        faint.putalpha(faint.getchannel('A').point(lambda a: min(a, 50)))
        faint.save(self.root / 'images/1.png')
        self.plan['frames'][1].pop('contactPoint')
        with self.assertRaisesRegex(ValueError, 'entire frame'):
            self.run_plan()

    def test_semitransparent_png_is_preserved_but_gif_is_binary(self):
        with Image.open(self.root / 'images/1.png') as image:
            translucent = image.convert('RGBA')
        translucent.putpixel((21, 21), (245, 245, 245, 150))
        translucent.save(self.root / 'images/1.png')
        self.run_plan()
        with Image.open(self.root / 'out/images/001.png') as png:
            self.assertEqual(png.getpixel((21, 21))[3], 150)
        self.assertEqual(self.gif_frames()[1].getpixel((21, 21))[3], 255)

    def test_opaque_black_is_not_mistaken_for_transparency(self):
        with Image.open(self.root / 'images/1.png') as image:
            marked = image.convert('RGBA')
        marked.putpixel((22, 22), (0, 0, 0, 255))
        marked.save(self.root / 'images/1.png')
        self.run_plan()
        self.assertEqual(self.gif_frames()[1].getpixel((22, 22)), (0, 0, 0, 255))

    def test_noninteger_scale_uses_measured_anchor(self):
        self.plan['scale'] = 0.7
        report = self.run_plan()
        for frame in report['frames']:
            self.assertLessEqual(abs(frame['contactPoint'][1] - 49), 0.5)

    def test_contact_cannot_be_transparent_space(self):
        self.plan['frames'][0]['contactPoint'] = [12, 49]
        with self.assertRaisesRegex(ValueError, 'contactPoint.*visible'):
            self.run_plan()

    def test_contact_cannot_be_inside_body(self):
        self.plan['frames'][0]['contactPoint'] = [32, 48]
        with self.assertRaisesRegex(ValueError, 'contactPoint.*lower boundary'):
            self.run_plan()

    def test_contact_cannot_disappear_when_downscaled(self):
        self.plan['scale'] = 0.5
        for item in self.plan['frames']:
            path = self.root / item['source']
            with Image.open(path) as original:
                frame = original.convert('RGBA')
            frame.putpixel((32, 56), (255, 255, 255, 255))
            frame.save(path)
            item['contactPoint'] = [32, 56]
            item['anchor'] = [32, 56]
        with self.assertRaisesRegex(ValueError, 'contactPoint.*scal'):
            self.run_plan()

    def test_required_prop_cannot_be_excluded_from_crop(self):
        self.plan['frames'][0]['crop'] = [15, 18, 27, 35]
        self.plan['frames'][0]['requiredRegions'] = [{'label': 'ball', 'box': [42, 15, 3, 3]}]
        with self.assertRaisesRegex(ValueError, 'required region.*outside crop'):
            self.run_plan()

    def test_required_prop_must_survive_alpha_threshold(self):
        with Image.open(self.root / 'images/0.png') as image:
            faint = image.convert('RGBA')
        ImageDraw.Draw(faint).rectangle((42, 15, 44, 17), fill=(230, 25, 30, 50))
        faint.save(self.root / 'images/0.png')
        self.plan['frames'][0]['requiredRegions'] = [{'label': 'ball', 'box': [42, 15, 3, 3]}]
        with self.assertRaisesRegex(ValueError, 'required region.*visible'):
            self.run_plan()

    def test_required_prop_coordinates_follow_per_frame_mirror(self):
        for index, frame in enumerate(self.plan['frames']):
            frame['requiredRegions'] = [{'label': 'ball', 'box': [42 + 3 * index, 15, 3, 3]}]
        self.plan.update(mirrorX=True, mirrorReason='Synthetic mirror fixture')
        report = self.run_plan()
        for index, frame in enumerate(self.gif_frames()):
            region = report['frames'][index]['requiredRegions'][0]
            self.assertEqual(region['outputBox'], [19 - 3 * index, 15, 22 - 3 * index, 18])
            self.assertEqual(frame.getchannel('A').crop(region['outputBox']).getextrema(), (255, 255))

    def test_source_change_during_encoding_is_rejected(self):
        def change_source(frames, threshold):
            Image.new('RGBA', (64, 64)).save(self.root / 'images/0.png')
            return indexed_frames(frames, threshold)
        with patch('compile_action.indexed_frames', side_effect=change_source):
            with self.assertRaisesRegex(ValueError, 'source changed during'):
                self.run_plan()
        self.assertFalse((self.root / 'out').exists())

    def test_plan_change_during_encoding_is_rejected(self):
        def change_plan(frames, threshold):
            self.plan['loop'] = False
            (self.root / 'plan.json').write_text(json.dumps(self.plan), encoding='utf-8')
            return indexed_frames(frames, threshold)
        with patch('compile_action.indexed_frames', side_effect=change_plan):
            with self.assertRaisesRegex(ValueError, 'plan changed during'):
                self.run_plan()
        self.assertFalse((self.root / 'out').exists())

    def test_bad_numbers_do_not_create_output(self):
        self.plan['scale'] = float('nan')
        with self.assertRaises(ValueError):
            self.run_plan()
        self.assertFalse((self.root / 'out').exists())

    def test_huge_integer_is_rejected_without_overflow(self):
        self.plan['scale'] = 10 ** 400
        with self.assertRaisesRegex(ValueError, 'scale'):
            self.run_plan()
        self.assertFalse((self.root / 'out').exists())

    def test_report_records_changed_source(self):
        report = self.run_plan('first')
        with Image.open(self.root / 'images/0.png') as image:
            changed = image.convert('RGBA')
        changed.putpixel((30, 30), (0, 255, 0, 255))
        changed.save(self.root / 'images/0.png')
        new = self.run_plan('second')
        self.assertNotEqual(report['sourceSha256']['images/0.png'], new['sourceSha256']['images/0.png'])


if __name__ == '__main__':
    unittest.main()
