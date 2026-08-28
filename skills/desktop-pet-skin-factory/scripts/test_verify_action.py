"""Evidence freshness tests with synthetic files; no fabricated real-world approval."""
import json
import unittest

from PIL import Image

import test_compile_action as fixtures
from verify_action import REVIEW_CHECKS, sha256, verify_action


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.CompileTests()
        self.fixture.setUp()
        self.root = self.fixture.root
        self.report = self.fixture.run_plan()
        self.plan = self.root / 'plan.json'
        self.run = self.root / 'out'

    def tearDown(self):
        self.fixture.tearDown()

    def edit(self, path, update):
        obj = json.loads(path.read_text(encoding='utf-8'))
        update(obj)
        path.write_text(json.dumps(obj), encoding='utf-8')

    def review(self):
        evidence = self.root / 'observations.txt'
        evidence.write_text('Synthetic test evidence, NOT real visual acceptance.', encoding='utf-8')
        value = {key: self.report[key] for key in ('actionId', 'planSha256', 'assetSha256', 'sourceSha256')}
        value.update(verdict='pass', observations='Synthetic test fixture only.',
                     checks={key: True for key in REVIEW_CHECKS},
                     evidence=[{'path': evidence.name, 'sha256': sha256(evidence)}])
        path = self.root / 'review.json'
        path.write_text(json.dumps(value), encoding='utf-8')
        return path

    def test_integrity_does_not_claim_visual_or_runtime_review(self):
        result = verify_action(self.plan, self.run)
        self.assertTrue(result['artifactsCurrent'])
        self.assertFalse(result['visualEvidenceCurrent'])
        self.assertFalse(result['runtimeVerified'])

    def test_complete_current_review_record(self):
        result = verify_action(self.plan, self.run, self.review())
        self.assertTrue(result['visualEvidenceCurrent'])
        self.assertFalse(result['runtimeVerified'])

    def test_powershell_uppercase_hashes_are_valid(self):
        path = self.review()
        def uppercase(record):
            for key in ('planSha256', 'assetSha256'):
                record[key] = record[key].upper()
            record['sourceSha256'] = {name: value.upper() for name, value in record['sourceSha256'].items()}
            for evidence in record['evidence']:
                evidence['sha256'] = evidence['sha256'].upper()
        self.edit(path, uppercase)
        self.assertTrue(verify_action(self.plan, self.run, path)['visualEvidenceCurrent'])

    def test_uppercase_report_hashes_are_valid(self):
        def uppercase(record):
            for key in ('planSha256', 'assetSha256'):
                record[key] = record[key].upper()
            for key in ('sourceSha256', 'artifactSha256'):
                record[key] = {name: value.upper() for name, value in record[key].items()}
        self.edit(self.run / 'report.json', uppercase)
        self.assertTrue(verify_action(self.plan, self.run)['artifactsCurrent'])

    def test_malformed_hashes_do_not_pass(self):
        for value in (None, 'G' * 64, 'a' * 63, [], True):
            with self.subTest(value=value):
                path = self.review()
                self.edit(path, lambda p: p.update(assetSha256=value))
                with self.assertRaisesRegex(ValueError, 'stale review'):
                    verify_action(self.plan, self.run, path)

    def test_source_change_invalidates_output(self):
        Image.new('RGBA', (64, 64)).save(self.root / 'images/0.png')
        with self.assertRaisesRegex(ValueError, 'source changed'):
            verify_action(self.plan, self.run)

    def test_timing_change_invalidates_output(self):
        self.edit(self.plan, lambda p: p['frames'][0].update(durationMs=200))
        with self.assertRaisesRegex(ValueError, 'plan changed'):
            verify_action(self.plan, self.run)

    def test_changed_gif_or_qa_or_frame_is_detected(self):
        for relative in ('gifs/fixture.gif', 'qa/dark.png', 'images/000.png', 'plan.json'):
            with self.subTest(relative=relative):
                path = self.run / relative
                original = path.read_bytes()
                path.write_bytes(original + b' ')
                try:
                    with self.assertRaisesRegex(ValueError, 'artifact changed'):
                        verify_action(self.plan, self.run)
                finally:
                    path.write_bytes(original)

    def test_incomplete_inventory_cannot_hide_missing_preview(self):
        self.edit(self.run / 'report.json', lambda p: p['artifactSha256'].pop('qa/dark.png'))
        with self.assertRaisesRegex(ValueError, 'inventory incomplete'):
            verify_action(self.plan, self.run)

    def test_failed_marker_is_a_hard_failure(self):
        (self.run / 'FAILED.txt').write_text('Interrupted', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'failed compilation'):
            verify_action(self.plan, self.run)

    def test_forged_source_plan_does_not_redirect_reads(self):
        self.edit(self.run / 'report.json', lambda p: p.update(sourcePlan='C:/unrelated/private.json'))
        self.assertTrue(verify_action(self.plan, self.run)['artifactsCurrent'])

    def test_stale_review_is_not_accepted(self):
        path = self.review()
        self.edit(path, lambda p: p.update(assetSha256='0' * 64))
        with self.assertRaisesRegex(ValueError, 'stale review'):
            verify_action(self.plan, self.run, path)

    def test_missing_slow_playback_is_not_passed(self):
        path = self.review()
        self.edit(path, lambda p: p['checks'].update(slowSpeed=False))
        with self.assertRaisesRegex(ValueError, 'visual checks incomplete'):
            verify_action(self.plan, self.run, path)

    def test_changed_evidence_is_not_accepted(self):
        path = self.review()
        (self.root / 'observations.txt').write_text('different evidence', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'evidence changed'):
            verify_action(self.plan, self.run, path)

    def test_empty_evidence_is_not_accepted(self):
        path = self.review()
        self.edit(path, lambda p: p.update(evidence=[]))
        with self.assertRaisesRegex(ValueError, 'evidence is missing'):
            verify_action(self.plan, self.run, path)

    def test_evidence_path_escape_is_rejected(self):
        path = self.review()
        self.edit(path, lambda p: p['evidence'][0].update(path='../outside.txt'))
        with self.assertRaisesRegex(ValueError, 'escapes'):
            verify_action(self.plan, self.run, path)


if __name__ == '__main__':
    unittest.main()
