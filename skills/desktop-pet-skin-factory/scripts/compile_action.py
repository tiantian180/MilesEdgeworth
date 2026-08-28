"""Compile approved RGBA artwork using explicit crops/anchors; never generate or key art.

Pillow is the only dependency. Outputs go to a NEW review directory, never a live skin.
"""
import argparse
import hashlib
from io import BytesIO
import json
import math
from pathlib import Path
import re
import sys

from PIL import Image, ImageChops, ImageDraw, ImageOps


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(value):
    # Python integers are finite but may overflow conversion to a C double.
    return type(value) is int or (type(value) is float and math.isfinite(value))


def vector(value, length, label, integer=False):
    require(isinstance(value, list) and len(value) == length, f'{label}: expected {length} numbers')
    require(all(number(v) and (not integer or type(v) is int) for v in value), f'{label}: invalid numbers')
    return value


def source_path(root, name):
    require(isinstance(name, str) and name and ':' not in name and '\\' not in name,
            'source must be a relative forward-slash path')
    path = (root / name).resolve()
    require(not Path(name).is_absolute() and path.is_relative_to(root), 'source escapes plan directory')
    require(path.is_file(), f'missing source: {name}')
    return path


def visible_hash(frame):
    return hashlib.sha256(frame.convert('RGBa').tobytes()).hexdigest()


def guarded(box, size, padding):
    return box and box[0] >= padding and box[1] >= padding and box[2] <= size[0] - padding and box[3] <= size[1] - padding


def required_regions(item, crop, alpha, threshold, index):
    """Check human-assigned prop regions, without guessing component ownership."""
    regions = item.get('requiredRegions', [])
    require(isinstance(regions, list), f'frame {index}: requiredRegions must be a list')
    x, y, w, h = crop
    checked, labels = [], set()
    for entry in regions:
        require(isinstance(entry, dict), 'required region must be an object')
        label = entry.get('label')
        require(isinstance(label, str) and label.strip() and label not in labels,
                'required region labels must be nonempty and unique within a frame')
        labels.add(label)
        rx, ry, rw, rh = vector(entry.get('box'), 4, 'required region box', True)
        require(rw > 0 and rh > 0 and x <= rx and y <= ry and rx + rw <= x + w and ry + rh <= y + h,
                f'frame {index}: required region {label} outside crop')
        box = (rx - x, ry - y, rx + rw - x, ry + rh - y)
        require(alpha.crop(box).getextrema()[1] >= threshold,
                f'frame {index}: required region {label} has no GIF-visible pixels')
        checked.append({'label': label, 'sourceBox': entry['box']})
    return checked


def prepare(plan_path):
    plan_bytes = plan_path.read_bytes()
    plan = json.loads(plan_bytes.decode('utf-8-sig'))
    require(isinstance(plan, dict), 'plan must be an object')
    require(isinstance(plan.get('actionId'), str) and re.fullmatch(r'[a-z0-9][a-z0-9-]{0,63}', plan['actionId']), 'invalid actionId')
    canvas = vector(plan.get('canvas'), 2, 'canvas', True)
    require(all(16 <= v <= 1024 for v in canvas), 'canvas dimensions must be 16..1024')
    target = vector(plan.get('targetAnchor'), 2, 'targetAnchor')
    require(all(0 <= v < limit for v, limit in zip(target, canvas)), 'targetAnchor outside canvas')
    scale = plan.get('scale')
    require(number(scale) and 0 < scale <= 8, 'scale must be >0 and <=8')
    padding = plan.get('padding')
    require(type(padding) is int and 1 <= padding < min(canvas) / 2, 'invalid padding')
    threshold = plan.get('alphaThreshold')
    require(type(threshold) is int and 1 <= threshold <= 255, 'alphaThreshold must be 1..255')
    require(type(plan.get('loop')) is bool, 'loop must be a boolean')
    mirror = plan.get('mirrorX', False)
    require(type(mirror) is bool, 'mirrorX must be a boolean')
    require(not mirror or isinstance(plan.get('mirrorReason'), str) and plan['mirrorReason'].strip(), 'mirror requires an approved mirrorReason')
    baseline = plan.get('baselineY')
    require(baseline is None or number(baseline) and 0 <= baseline < canvas[1], 'invalid baselineY')
    frames = plan.get('frames')
    require(isinstance(frames, list) and 2 <= len(frames) <= 96, 'expected 2..96 frames')
    require(canvas[0] * canvas[1] * len(frames) <= 32_000_000, 'batch exceeds 32 million frame pixels; split the action or reduce canvas')
    output, records, sources = [], [], {}
    contacts = 0
    for index, item in enumerate(frames):
        require(isinstance(item, dict), f'frame {index}: expected object')
        path = source_path(plan_path.parent, item.get('source'))
        crop = vector(item.get('crop'), 4, f'frame {index} crop', True)
        x, y, w, h = crop
        anchor = vector(item.get('anchor'), 2, f'frame {index} anchor')
        require(x <= anchor[0] < x + w and y <= anchor[1] < y + h, f'frame {index}: anchor outside crop')
        delay = item.get('durationMs')
        require(type(delay) is int and 20 <= delay <= 10000 and delay % 10 == 0,
                f'frame {index}: durationMs must be 20..10000 in 10ms steps')
        source_bytes = path.read_bytes()
        digest = hashlib.sha256(source_bytes).hexdigest()
        require(item['source'] not in sources or sources[item['source']] == digest,
                f'frame {index}: source changed during compilation')
        sources[item['source']] = digest
        with Image.open(BytesIO(source_bytes)) as source:
            require(source.format == 'PNG' and getattr(source, 'n_frames', 1) == 1, f'frame {index}: source must be a still PNG')
            require('A' in source.getbands() or 'transparency' in source.info, f'frame {index}: source has no alpha; use imagegen extraction')
            require(x >= 0 and y >= 0 and w > 0 and h > 0 and x + w <= source.width and y + h <= source.height, f'frame {index}: crop outside source')
            region = source.crop((x, y, x + w, y + h)).convert('RGBA')
        source_alpha = region.getchannel('A')
        bounds = source_alpha.getbbox()
        require(guarded(bounds, region.size, 1), f'frame {index}: empty or visible pixels touch source crop; recover complete silhouette first')
        required = required_regions(item, crop, source_alpha, threshold, index)
        dest_size = (max(1, round(w * scale)), max(1, round(h * scale)))
        require(max(dest_size) <= 4096, f'frame {index}: scaled crop is too large')
        region = region.resize(dest_size, Image.Resampling.LANCZOS)
        # Use actual rounded resize factors for anchors, not the nominal scale.
        sx, sy = dest_size[0] / w, dest_size[1] / h
        dx = round(target[0] - (anchor[0] - x) * sx)
        dy = round(target[1] - (anchor[1] - y) * sy)
        box = region.getchannel('A').getbbox()
        require(box is not None, f'frame {index}: resize removed all visible pixels')
        placed = (box[0] + dx, box[1] + dy, box[2] + dx, box[3] + dy)
        require(guarded(placed, canvas, padding), f'frame {index}: output clips or violates padding; adjust shared scale/canvas')
        frame = Image.new('RGBA', tuple(canvas))
        frame.alpha_composite(region, (dx, dy))
        contact = item.get('contactPoint')
        mapped_contact = None
        if contact is not None:
            vector(contact, 2, f'frame {index} contactPoint', True)
            require(x <= contact[0] < x + w and y <= contact[1] < y + h, f'frame {index}: contactPoint outside crop')
            require(baseline is not None, 'contactPoint requires baselineY')
            mapped_contact = [dx + (contact[0] - x) * sx, dy + (contact[1] - y) * sy]
            require(abs(mapped_contact[1] - baseline) <= 2, f'frame {index}: planted-foot baseline drift exceeds 2px')
            cx, cy = contact[0] - x, contact[1] - y
            require(source_alpha.getpixel((cx, cy)) >= threshold,
                    f'frame {index}: contactPoint must be a visible pixel, not empty space')
            require(cy + 1 < h and source_alpha.getpixel((cx, cy + 1)) < threshold,
                    f'frame {index}: contactPoint must be on a lower boundary, not inside the body')
            # A source pixel can vanish during resampling even though its mapped
            # coordinate still matches baselineY. Inspect actual output pixels.
            px, py = (round(value) for value in mapped_contact)
            contact_box = (px - 1, py - 1, px + 2, py + 2)
            require(frame.getchannel('A').crop(contact_box).getextrema()[1] >= threshold,
                    f'frame {index}: contactPoint has no visible pixels after scaling')
            contacts += 1
        for entry in required:
            rx, ry, rw, rh = entry['sourceBox']
            # Check the assigned region after scaling, not just the whole frame.
            mapped = [math.floor(dx + (rx - x) * sx), math.floor(dy + (ry - y) * sy),
                      math.ceil(dx + (rx + rw - x) * sx), math.ceil(dy + (ry + rh - y) * sy)]
            require(frame.getchannel('A').crop(mapped).getextrema()[1] >= threshold,
                    f'frame {index}: required region {entry["label"]} lost visible pixels after scaling')
            if mirror:
                mapped[0], mapped[2] = canvas[0] - mapped[2], canvas[0] - mapped[0]
            entry['outputBox'] = mapped
        if mirror:
            frame = ImageOps.mirror(frame)
            if mapped_contact:
                mapped_contact[0] = canvas[0] - 1 - mapped_contact[0]
        output.append(frame)
        records.append({'index': index, 'source': item['source'], 'crop': crop,
                        'sourceBoundsInCrop': list(bounds), 'outputBounds': list(frame.getchannel('A').getbbox()),
                        'contactPoint': mapped_contact, 'requiredRegions': required, 'durationMs': delay})
    require(baseline is None or contacts > 0, 'baselineY requires at least one measured contactPoint')
    require(len({visible_hash(frame) for frame in output}) >= 2, 'all frames are identical; no animation to compile')
    return plan, output, records, sources, hashlib.sha256(plan_bytes).hexdigest()


def indexed_rgba(frame):
    """Decode our reserved slot without mutating a Pillow palette via info.alpha."""
    rgba = frame.convert('RGBA')
    rgba.putalpha(frame.point([255] * 255 + [0], mode='L'))
    return rgba


def indexed_frames(frames, threshold):
    # Shared palette from EVERY full-resolution frame; 255 is reserved for alpha.
    strip = Image.new('RGB', (frames[0].width, frames[0].height * len(frames)))
    for i, frame in enumerate(frames):
        rgb = frame.convert('RGB')
        rgb.paste((0, 0, 0), mask=frame.getchannel('A').point(lambda a: 255 if a < threshold else 0))
        strip.paste(rgb, (0, i * frame.height))
    palette = strip.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    # Low-color fixtures produce a SHORT palette. Index 255 still needs an
    # actual table entry or the GIF header/table length can become inconsistent.
    colors = palette.getpalette()
    # Pad with color zero, so remapping a padded index 255 to zero preserves RGB.
    palette.putpalette(colors + colors[:3] * ((768 - len(colors)) // 3))
    result = []
    for frame in frames:
        quantized = frame.convert('RGB').quantize(palette=palette, dither=Image.Dither.NONE)
        # Quantizers can choose padded palette entries: reserve 255 explicitly.
        quantized = quantized.point([i if i != 255 else 0 for i in range(256)])
        quantized.paste(255, mask=frame.getchannel('A').point(lambda a: 255 if a < threshold else 0))
        require(indexed_rgba(quantized).getchannel('A').getbbox(),
                'alpha threshold removed an entire frame')
        result.append(quantized)
    require(len({visible_hash(indexed_rgba(frame)) for frame in result}) >= 2,
            'palette/alpha threshold collapsed animation into one visible frame')
    require(all(visible_hash(indexed_rgba(a)) != visible_hash(indexed_rgba(b))
                for a, b in zip(result, result[1:])),
            'adjacent frames are identical after encoding; use one frame with a longer duration')
    return result


def decode_gif(path, expected, delays, loop):
    decoded, actual_delays = [], []
    with Image.open(path) as movie:
        require((movie.info.get('loop') == 0) if loop else ('loop' not in movie.info), 'incorrect GIF repeat metadata')
        for index in range(movie.n_frames):
            movie.seek(index)
            decoded.append(movie.convert('RGBA'))
            actual_delays.append(movie.info.get('duration', 0))
    require(len(decoded) == len(expected), 'encoder merged frames unexpectedly')
    require(actual_delays == delays, 'GIF frame timing changed during encoding')
    for index, (actual, wanted) in enumerate(zip(decoded, expected)):
        require(actual.size == wanted.size and visible_hash(actual) == visible_hash(wanted),
                f'GIF frame {index}: palette/disposal/alpha round-trip mismatch')
    return decoded


def contact_sheet(frames, delays, background):
    tile = 160
    columns = min(4, len(frames))
    sheet = Image.new('RGB', (columns * tile, math.ceil(len(frames) / columns) * (tile + 24)), background)
    draw = ImageDraw.Draw(sheet)
    for i, frame in enumerate(frames):
        thumb = ImageOps.contain(frame, (tile - 12, tile - 12))
        x, y = (i % columns) * tile, (i // columns) * (tile + 24)
        sheet.paste(thumb, (x + (tile - thumb.width) // 2, y + (tile - thumb.height) // 2), thumb)
        draw.text((x + 6, y + tile), f'{i:02d} | {delays[i]} ms', fill='#888888')
    return sheet


def compile_action(plan_path, output_dir):
    plan_path, output_dir = Path(plan_path).resolve(), Path(output_dir).resolve()
    require(not output_dir.exists(), 'output directory already exists; use a new action revision directory')
    plan, frames, records, sources, plan_hash = prepare(plan_path)
    indexed = indexed_frames(frames, plan['alphaThreshold'])
    expected = [indexed_rgba(frame) for frame in indexed]
    delays = [record['durationMs'] for record in records]
    require(hashlib.sha256(plan_path.read_bytes()).hexdigest() == plan_hash, 'plan changed during compilation')
    for name, digest in sources.items():
        require(hashlib.sha256(source_path(plan_path.parent, name).read_bytes()).hexdigest() == digest,
                f'source changed during compilation: {name}')
    output_dir.mkdir(parents=True, exist_ok=False)
    try:
        for folder in ('images', 'gifs', 'qa'):
            (output_dir / folder).mkdir()
        # Keep normalized RGBA frames separate from GIFs, and preserve all sources.
        for index, frame in enumerate(frames):
            frame.save(output_dir / 'images' / f'{index:03d}.png')
        gif = output_dir / 'gifs' / f"{plan['actionId']}.gif"
        options = {'loop': 0} if plan['loop'] else {}
        indexed[0].save(gif, save_all=True, append_images=indexed[1:], duration=delays,
                        disposal=2, transparency=255, background=255, optimize=False, **options)
        decoded = decode_gif(gif, expected, delays, plan['loop'])
        for name, color in (('light', '#f5f5f7'), ('dark', '#202126')):
            contact_sheet(decoded, delays, color).save(output_dir / 'qa' / f'{name}.png')
        for index, frame in enumerate(decoded):
            require(guarded(frame.getchannel('A').getbbox(), frame.size, plan['padding']), f'encoded frame {index} is empty or clipped')
            for region in records[index]['requiredRegions']:
                require(frame.getchannel('A').crop(region['outputBox']).getextrema()[1] == 255,
                        f'encoded frame {index}: missing required region {region["label"]}')
        warnings = ['Visual and runtime review are NOT performed by this compiler.',
                    'Pixel differences do not prove distinct poses or coherent anatomy.',
                    'GIF has binary alpha; inspect fur and effects on both backgrounds.']
        seam = ImageChops.difference(decoded[-1].convert('RGBa'), decoded[0].convert('RGBa'))
        seam_box = seam.getbbox(alpha_only=False)
        if plan['loop'] and seam_box:
            warnings.append('Loop endpoints differ: inspect last-to-first motion before acceptance.')
        report = {'status': 'compiled', 'actionId': plan['actionId'], 'sourceSha256': sources,
                  'sourcePlan': str(plan_path), 'assetSha256': hashlib.sha256(gif.read_bytes()).hexdigest(),
                  'planSha256': plan_hash,
                  'inputFrames': len(frames), 'encodedFrames': len(decoded),
                  'distinctPixelFrames': len({visible_hash(frame) for frame in decoded}),
                  'distinctPoses': None, 'durationMs': sum(delays), 'loop': plan['loop'],
                  'firstLastDifferenceBounds': seam_box, 'frames': records,
                  'checks': {'encoding': 'pass', 'visual': None, 'runtime': None}, 'warnings': warnings}
        (output_dir / 'plan.json').write_text(json.dumps(plan, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        report['artifactSha256'] = {path.relative_to(output_dir).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                                    for path in sorted(output_dir.rglob('*')) if path.is_file()}
        (output_dir / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        return report
    except Exception as error:
        (output_dir / 'FAILED.txt').write_text(str(error) + '\nDo not deploy these outputs. Sources are unchanged.\n', encoding='utf-8')
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plan', type=Path)
    parser.add_argument('--out', required=True, type=Path, help='new, non-existing revision directory')
    args = parser.parse_args()
    try:
        report = compile_action(args.plan, args.out)
    except (OSError, ValueError, TypeError, KeyError, Image.DecompressionBombError) as error:
        print(f'compile failed: {error}', file=sys.stderr)
        return 1
    print(json.dumps(report, ensure_ascii=True, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
