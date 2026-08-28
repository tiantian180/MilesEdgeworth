"""Check that an action's source files, exports and playback notes match its current version."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

from compile_action import require, source_path


REVIEW_CHECKS = ('lightBackground', 'darkBackground', 'actualSize', 'normalSpeed',
                 'slowSpeed', 'loopOrExit', 'propsAndSilhouette', 'identityAndMotion')


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def same_digest(left, right):
    # PowerShell Get-FileHash emits uppercase; SHA256 hex is case-insensitive.
    return (isinstance(left, str) and isinstance(right, str)
            and re.fullmatch(r'[0-9a-fA-F]{64}', left) is not None
            and re.fullmatch(r'[0-9a-fA-F]{64}', right) is not None
            and left.lower() == right.lower())


def same_sources(left, right):
    return (isinstance(left, dict) and isinstance(right, dict) and set(left) == set(right)
            and all(same_digest(left[name], right[name]) for name in left))


def read_object(path):
    value = json.loads(path.read_text(encoding='utf-8-sig'))
    require(isinstance(value, dict), f'{path.name}: expected JSON object')
    return value


def verify_action(plan_path, run_dir, review_path=None):
    plan_path, run_dir = Path(plan_path).resolve(), Path(run_dir).resolve()
    require(run_dir.is_dir(), 'run directory is missing')
    require(not (run_dir / 'FAILED.txt').exists(), 'failed compilation must not be accepted')
    plan = read_object(plan_path)
    report = read_object(source_path(run_dir, 'report.json'))
    snapshot = read_object(source_path(run_dir, 'plan.json'))
    action_id = plan.get('actionId')
    require(isinstance(action_id, str) and re.fullmatch(r'[a-z0-9][a-z0-9-]{0,63}', action_id), 'invalid actionId')
    require(report.get('actionId') == action_id, 'report actionId mismatch')
    require(report.get('status') == 'compiled' and isinstance(report.get('checks'), dict)
            and report['checks'].get('encoding') == 'pass', 'report does not record a successful compilation')
    require(same_digest(sha256(plan_path), report.get('planSha256')) and plan == snapshot,
            'plan changed; recompile before review')
    frames = plan.get('frames')
    require(isinstance(frames, list) and 2 <= len(frames) <= 96 and all(isinstance(f, dict) for f in frames), 'invalid frames')
    names = {frame['source'] for frame in frames}
    require(isinstance(report.get('sourceSha256'), dict) and names == set(report['sourceSha256']),
            'source inventory mismatch')
    for name in sorted(names):
        path = source_path(plan_path.parent, name)
        require(same_digest(sha256(path), report['sourceSha256'][name]), f'source changed: {name}')
    # Derive the complete expected inventory; a shortened report cannot hide a
    # missing frame or QA image. Old reports must be regenerated, not trusted.
    expected = {'plan.json', 'qa/light.png', 'qa/dark.png', f'gifs/{action_id}.gif'}
    expected.update(f'images/{index:03d}.png' for index in range(len(frames)))
    inventory = report.get('artifactSha256')
    require(isinstance(inventory, dict) and set(inventory) == expected,
            'artifact inventory incomplete; regenerate with the current compiler')
    for name in sorted(expected):
        path = source_path(run_dir, name)
        require(same_digest(sha256(path), inventory[name]), f'compiled artifact changed: {name}')
    require(same_digest(report.get('assetSha256'), inventory[f'gifs/{action_id}.gif']), 'asset hash mismatch')
    result = {'actionId': action_id, 'artifactsCurrent': True, 'visualEvidenceCurrent': False,
              'runtimeVerified': False, 'warnings': ['File versions are checked here; preview the animation in the target player.']}
    if review_path is not None:
        review_path = Path(review_path).resolve()
        review = read_object(review_path)
        require(review.get('actionId') == action_id and review.get('verdict') == 'pass', 'visual review is not passed for this action')
        for field in ('planSha256', 'assetSha256'):
            require(same_digest(review.get(field), report[field]), f'stale review: {field} differs')
        require(same_sources(review.get('sourceSha256'), report['sourceSha256']),
                'stale review: sourceSha256 differs')
        checks = review.get('checks')
        require(isinstance(checks, dict) and all(checks.get(key) is True for key in REVIEW_CHECKS),
                'visual checks incomplete: require light/dark, actual size, speeds, ending, props and motion')
        require(isinstance(review.get('observations'), str) and review['observations'].strip(),
                'visual review needs concrete observations')
        evidence = review.get('evidence')
        require(isinstance(evidence, list) and evidence, 'visual evidence is missing')
        for record in evidence:
            require(isinstance(record, dict), 'invalid evidence record')
            path = source_path(review_path.parent, record.get('path'))
            require(path.stat().st_size > 0 and same_digest(sha256(path), record.get('sha256')),
                    f'evidence changed or empty: {record.get("path")}')
        result['visualEvidenceCurrent'] = True
    else:
        result['warnings'].append('Playback notes were not supplied; only exported files were checked.')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plan', type=Path, help='explicit current plan, not a path trusted from the report')
    parser.add_argument('run', type=Path)
    parser.add_argument('--review', type=Path, help='explicit completed visual review to validate')
    args = parser.parse_args()
    try:
        result = verify_action(args.plan, args.run, args.review)
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(json.dumps({'passed': False, 'error': str(error)}, ensure_ascii=True))
        return 1
    print(json.dumps({'passed': True, **result}, ensure_ascii=True, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
