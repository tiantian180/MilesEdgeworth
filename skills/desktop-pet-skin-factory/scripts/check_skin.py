"""Check DesktopCat skin files, references and PNG/GIF output without changing them."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
from urllib.parse import unquote

try:
    from PIL import Image, ImageChops
except ImportError:
    print('Missing Pillow. Install it in the approved Python environment.', file=sys.stderr)
    raise SystemExit(2)


def local_refs(value):
    if isinstance(value, dict):
        for item in value.values():
            yield from local_refs(item)
    elif isinstance(value, list):
        for item in value:
            yield from local_refs(item)
    elif isinstance(value, str) and value.startswith('file:'):
        yield value[5:]


def inside_file(root, relative):
    if not isinstance(relative, str) or not relative:
        raise ValueError('local asset path must be a nonempty string')
    relative = unquote(relative).replace('\\', '/')
    if relative.startswith('/') or ':' in relative:
        raise ValueError(f'non-relative local asset: {relative}')
    path = (root / relative).resolve()
    if not path.is_relative_to(root):
        raise ValueError(f'asset escapes skin root: {relative}')
    if not path.is_file():
        raise ValueError(f'missing file: {relative}')
    return path


def visible_hash(frame):
    # Premultiplied pixels ignore arbitrary RGB underneath fully transparent alpha.
    return hashlib.sha256(frame.convert('RGBa').tobytes()).hexdigest()


def inspect_image(path, relative, args):
    errors, warnings = [], []
    result = {'path': relative, 'errors': errors, 'warnings': warnings}
    with Image.open(path) as image:
        actual = image.format
        expected = 'GIF' if path.suffix.lower() == '.gif' else 'PNG'
        if actual != expected:
            errors.append(f'extension is {expected}, decoded format is {actual}')
        result['size'] = list(image.size)
        is_gif = expected == 'GIF'
        count = getattr(image, 'n_frames', 1)
        if is_gif and args.canvas and image.size != tuple(args.canvas):
            errors.append(f'canvas differs from expected {args.canvas}')
        first = last = None
        hashes = set()
        duration = 0
        coverage = []
        boxes = []
        bad_alpha, bad_edge, empty, bad_duration = [], [], [], []
        for index in range(count):
            image.seek(index)
            frame = image.convert('RGBA')
            alpha = frame.getchannel('A')
            lo, hi = alpha.getextrema()
            if lo != 0:
                bad_alpha.append(index)
            if hi == 0:
                empty.append(index)
            box = alpha.getbbox()
            boxes.append(list(box) if box else None)
            if box and (box[0] < args.padding or box[1] < args.padding
                        or box[2] > frame.width - args.padding
                        or box[3] > frame.height - args.padding):
                bad_edge.append(index)
            histogram = alpha.histogram()
            coverage.append(round(1 - histogram[0] / (frame.width * frame.height), 4))
            digest = visible_hash(frame)
            hashes.add(digest)
            if first is None:
                first = frame.copy()
            last = frame.copy()
            if is_gif:
                delay = image.info.get('duration', 0)
                if not isinstance(delay, (int, float)) or delay < 20:
                    bad_duration.append(index)
                else:
                    duration += delay
        if bad_alpha:
            errors.append(f'no fully transparent pixels in frames {bad_alpha[:12]}')
        if empty:
            errors.append(f'empty frames {empty[:12]}')
        if bad_edge:
            message = f'visible pixels inside {args.padding}px edge guard: frames {bad_edge[:12]}'
            # Static props may intentionally be tightly cropped; frame clipping
            # is a hard GIF error, while static boundaries need visual judgment.
            (errors if is_gif else warnings).append(message)
        if bad_duration:
            errors.append(f'missing or sub-20ms GIF delays: frames {bad_duration[:12]}')
        if max(coverage, default=0) > 0.95:
            warnings.append('over 95% opaque coverage; inspect for baked checkerboard/background')
        result.update(encodedFrames=count, distinctPixelFrames=len(hashes),
                      durationMs=duration if is_gif else None,
                      opaqueCoverageRange=[min(coverage), max(coverage)], bounds=boxes)
        if is_gif:
            if count < 2:
                errors.append('GIF is a still image')
            if len(hashes) < args.min_poses:
                errors.append(f'only {len(hashes)} distinct pixel frames, expected >= {args.min_poses}')
            result['firstEqualsLast'] = visible_hash(first) == visible_hash(last)
            difference = ImageChops.difference(first.convert('RGBa'), last.convert('RGBa'))
            result['firstLastDifferenceBounds'] = difference.getbbox(alpha_only=False)
            warnings.append('Distinct pixel frames do not prove distinct poses or smooth motion; visual review required.')
            if not result['firstEqualsLast']:
                warnings.append('Start/end drawings differ; check intended one-shot ending or loop seam.')
    return result


def check_skin(root_arg, args):
    root = Path(root_arg).resolve()
    errors, warnings, images = [], [], []
    result = {'skinRoot': str(root), 'errors': errors, 'warnings': warnings, 'assets': images}
    if not root.is_dir():
        errors.append('skin directory does not exist')
        return result
    docs = []
    try:
        descriptor = json.loads(inside_file(root, 'skin.json').read_text(encoding='utf-8-sig'))
        if not isinstance(descriptor, dict):
            raise ValueError('skin.json must be an object')
        manifest_name = descriptor['manifest']
        manifest = json.loads(inside_file(root, manifest_name).read_text(encoding='utf-8-sig'))
        if not isinstance(manifest, dict):
            raise ValueError('manifest must be an object')
        docs = [descriptor, manifest]
        for name, doc in (('skin.json', descriptor), ('manifest', manifest)):
            if not isinstance(doc.get('id'), str) or not doc['id'].strip():
                errors.append(f'{name} id must be a nonempty string')
        if descriptor.get('id') != manifest.get('id'):
            errors.append('skin.json and manifest ids differ')
    except (OSError, ValueError, KeyError, TypeError) as error:
        errors.append(f'invalid skin metadata: {error}')
    refs = set()
    for doc in docs:
        for ref in local_refs(doc):
            try:
                refs.add(inside_file(root, ref))
            except (OSError, ValueError) as error:
                errors.append(str(error))
    assets = root / 'assets'
    candidates = set()
    if not assets.is_dir():
        errors.append('missing assets directory')
    else:
        candidates = {p for p in assets.rglob('*') if p.is_file() and p.suffix.lower() in ('.png', '.gif')}
    candidates.update(p for p in refs if p.suffix.lower() in ('.png', '.gif'))
    for candidate in sorted(candidates):
        path = candidate.resolve()
        relative = candidate.relative_to(root).as_posix()
        if not path.is_relative_to(root):
            errors.append(f'asset symlink escapes skin root: {relative}')
            continue
        folder = 'gifs' if path.suffix.lower() == '.gif' else 'images'
        if not relative.startswith(f'assets/{folder}/'):
            errors.append(f'wrong folder: {relative}; expected assets/{folder}/')
        if path not in refs:
            warnings.append(f'unreferenced image asset: {relative}')
        try:
            images.append(inspect_image(path, relative, args))
        except (OSError, ValueError, EOFError, Image.DecompressionBombError) as error:
            errors.append(f'cannot decode {relative}: {error}')
    if not images:
        errors.append('no decodable PNG/GIF assets')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('skins', nargs='+', help='explicit DesktopCat skin roots')
    parser.add_argument('--min-poses', type=int, default=2, help='minimum different visible images per GIF')
    parser.add_argument('--padding', type=int, default=4, help='required transparent border in pixels')
    parser.add_argument('--canvas', nargs=2, type=int, metavar=('WIDTH', 'HEIGHT'), help='expected GIF canvas only')
    args = parser.parse_args()
    if args.min_poses < 2 or args.padding < 0 or (args.canvas and min(args.canvas) < 1):
        parser.error('min-poses >= 2, padding >= 0, positive canvas dimensions required')
    reports = [check_skin(root, args) for root in args.skins]
    failures = sum(len(r['errors']) + sum(len(a['errors']) for a in r['assets']) for r in reports)
    print(json.dumps({'passed': failures == 0, 'errorCount': failures, 'skins': reports}, ensure_ascii=True, indent=2))
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())
