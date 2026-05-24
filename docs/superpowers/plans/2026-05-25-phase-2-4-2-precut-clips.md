# Phase 2.4.2 Precut Clips Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace runtime `frameRange` playback with build-time GIF clip generation, and migrate the Miles skin manifest/runtime model to schema v4 naming (`file:`, `clip`, `clips`, `animationPools`).

**Architecture:** Build-time tooling reads manifest `clips` definitions and writes complete GIF files to `generated/clips/`, plus a generated qrc manifest for built-in packaging. Runtime only receives complete GIF URLs and always plays them with `QMovie`. `SkinManifestLoader` becomes the schema boundary: `file:` URLs resolve under the skin root, bare `clip` names resolve to `generated/clips/{name}.gif`, and missing generated clips reject the manifest. Runtime naming is aligned with schema v4 by renaming `ActionPool*` to `AnimationPool*`, while JSON request keys still use `"pool"` because they name the request target, not the C++ type.

**Tech Stack:** Python 3 + Pillow for GIF slicing, Qt 6 / C++17 / QWidget / QMovie for runtime playback, CMake AUTORCC for qrc packaging, CTest smoke tests, Python static contract checks.

---

## Scope Check

This plan is Phase 2.4.2 only. It deliberately does not change ChatController segment queue logic, Go SSE event types, Langfuse tracing, or Phase 2.4 cleanFinish behavior except where tests must be updated after removing runtime frame ranges.

The implementable requirements are:

- Generate all top-level manifest `clips` into `generated/clips/{clipName}.gif` before runtime.
- Do not commit `generated/` artifacts.
- Include generated clips in the built-in Miles qrc at build time without manually listing each clip in `pet_assets.qrc`.
- For filesystem skins, require pre-generated `generated/clips/` and fail loudly when missing.
- Remove `QImageReader + setPixmap` manual frame playback from `PetSurfaceWindow`.
- Remove runtime `frameStart/frameEnd` from `AnimationVariant` and `PetRuntime`.
- Migrate Miles manifest to schema v4: `file:`, `clip`, `clips`, `animationPools`.
- Rename C++ `ActionPool*` concepts to `AnimationPool*`.
- Preserve current Phase 2.4 behavior: thinking/talking enter -> loop -> exit still works, but via pre-cut GIF files.

## File Structure

### Build Tooling

- Create: `tools/requirements.txt`
- Create: `tools/split_manifest_clips.py`
- Create: `tests/test_split_manifest_clips.py`
- Modify: `.gitignore`
- Responsibility: deterministic GIF slicing from manifest `clips` definitions.

### Build Integration

- Modify: `apps/desktop/CMakeLists.txt`
- Responsibility: generate Miles clips and `generated/clips.qrc` before qrc compilation, then package generated clips under `qrc:/skins/miles-edgeworth/generated/clips/...`.

### Manifest Model And Loader

- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
- Responsibility: schema v4 URL parsing, generated clip validation, `animationPools` parsing.

### Runtime Naming

- Rename: `apps/desktop/src/pet/selection/ActionPoolSelector.h` -> `apps/desktop/src/pet/selection/AnimationPoolSelector.h`
- Rename: `apps/desktop/src/pet/selection/ActionPoolSelector.cpp` -> `apps/desktop/src/pet/selection/AnimationPoolSelector.cpp`
- Modify: `apps/desktop/src/pet/requests/ActionRequest.h`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/src/pet/selection/ExpressionMappingResolver.cpp`
- Modify: `apps/desktop/src/pet/interaction/InteractionPipeline.cpp`
- Modify: `apps/desktop/CMakeLists.txt`
- Responsibility: align runtime names with `AnimationPool`, while preserving JSON request key `"pool"`.

### Surface Playback

- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.h`
- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Responsibility: remove runtime frame-range state and manual pixmap playback path.

### Miles Manifest

- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Responsibility: schema v4 manifest using top-level `clips`, `clip` variant references, `file:` resource URLs, and `animationPools`.

### Contract Checks And Docs

- Create: `tests/check_phase_2_4_2_precut_clips.py`
- Modify: `README.md`
- Responsibility: prevent regression to runtime frame slicing or v3 manifest names.

## Task 1: GIF Clip Slicing Tool

**Files:**

- Create: `tools/requirements.txt`
- Create: `tools/split_manifest_clips.py`
- Create: `tests/test_split_manifest_clips.py`
- Modify: `.gitignore`

- [ ] **Step 1: Add the tool dependency**

Create `tools/requirements.txt`:

```text
Pillow>=10.0.0
```

- [ ] **Step 2: Write the failing script test**

Create `tests/test_split_manifest_clips.py`:

```python
#!/usr/bin/env python3
import json
import shutil
import subprocess
import sys
from pathlib import Path

try:
    from PIL import Image, ImageSequence
except ImportError as exc:
    raise SystemExit("Pillow is required. Run: python3 -m pip install -r tools/requirements.txt") from exc

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "split_manifest_clips.py"
TMP = ROOT / ".tmp_phase_2_4_2_clip_test"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def make_source_gif(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = []
    for index in range(6):
        image = Image.new("RGBA", (4, 4), (index * 30, 0, 255 - index * 30, 255))
        frames.append(image)
    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=[11, 22, 33, 44, 55, 66],
        loop=0,
    )


def frame_count(path: Path) -> int:
    with Image.open(path) as image:
        return sum(1 for _ in ImageSequence.Iterator(image))


def frame_durations(path: Path) -> list[int]:
    with Image.open(path) as image:
        return [frame.info.get("duration", 0) for frame in ImageSequence.Iterator(image)]


def main() -> None:
    if TMP.exists():
        shutil.rmtree(TMP)
    skin = TMP / "test-skin"
    source = skin / "assets" / "body" / "raw" / "source.gif"
    make_source_gif(source)

    (skin / "manifest.json").write_text(json.dumps({
        "clips": {
            "talking.loop.right": {
                "source": "file:assets/body/raw/source.gif",
                "frameRange": [2, 4],
            }
        }
    }), encoding="utf-8")

    stale = skin / "generated" / "clips" / "stale.gif"
    stale.parent.mkdir(parents=True, exist_ok=True)
    stale.write_bytes(b"stale")

    result = subprocess.run(
        [sys.executable, str(SCRIPT), str(skin)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    require(result.returncode == 0, result.stderr)

    output = skin / "generated" / "clips" / "talking.loop.right.gif"
    qrc = skin / "generated" / "clips.qrc"
    require(output.is_file(), "generated clip should be written")
    require(qrc.is_file(), "generated qrc should be written")
    require(not stale.exists(), "script should clean stale generated clips before writing")
    require(frame_count(output) == 3, "1-based inclusive [2, 4] should generate 3 frames")
    require(frame_durations(output) == [22, 33, 44], "generated clip should preserve source frame durations")
    qrc_text = qrc.read_text(encoding="utf-8")
    require('<qresource prefix="/skins/test-skin/generated/clips">' in qrc_text, qrc_text)
    require('<file alias="talking.loop.right.gif">clips/talking.loop.right.gif</file>' in qrc_text, qrc_text)

    bad_manifest = skin / "manifest.json"
    bad_manifest.write_text(json.dumps({
        "clips": {
            "../escape": {
                "source": "file:assets/body/raw/source.gif",
                "frameRange": [1, 1],
            }
        }
    }), encoding="utf-8")
    bad = subprocess.run(
        [sys.executable, str(SCRIPT), str(skin)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    require(bad.returncode != 0, "unsafe clip name should fail")
    require("invalid clip name" in bad.stderr, bad.stderr)

    shutil.rmtree(TMP)
    print("phase 2.4.2 clip slicing tool ok")


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Run the script test to verify it fails**

Run:

```bash
python3 tests/test_split_manifest_clips.py
```

Expected: FAIL with a message containing `can't open file` or `No such file` for `tools/split_manifest_clips.py`.

- [ ] **Step 4: Implement the slicing script**

Create `tools/split_manifest_clips.py`:

```python
#!/usr/bin/env python3
import json
import html
import re
import shutil
import sys
from pathlib import Path

try:
    from PIL import Image, ImageSequence
except ImportError as exc:
    raise SystemExit("Pillow is required. Run: python3 -m pip install -r tools/requirements.txt") from exc

CLIP_NAME_RE = re.compile(r"^[A-Za-z0-9_.-]+$")


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(1)


def load_manifest(skin_root: Path) -> dict:
    manifest_path = skin_root / "manifest.json"
    if not manifest_path.is_file():
        fail(f"manifest not found: {manifest_path}")
    return json.loads(manifest_path.read_text(encoding="utf-8"))


def source_path_for_url(skin_root: Path, raw_url: str) -> Path:
    raw_url = raw_url.strip()
    if not raw_url.startswith("file:"):
        fail(f"clip source must use file: URL, got: {raw_url}")
    relative = raw_url[len("file:"):].lstrip("/")
    if not relative or ".." in Path(relative).parts:
        fail(f"clip source must stay under skin root, got: {raw_url}")
    path = (skin_root / relative).resolve()
    root = skin_root.resolve()
    if root not in path.parents and path != root:
        fail(f"clip source escapes skin root: {raw_url}")
    if not path.is_file():
        fail(f"clip source not found: {raw_url}")
    return path


def output_path_for_clip_id(output_dir: Path, clip_id: str) -> Path:
    if not clip_id or not CLIP_NAME_RE.fullmatch(clip_id):
        fail(f"invalid clip name: {clip_id}")
    return output_dir / f"{clip_id}.gif"


def frame_range_from_manifest(value: object) -> tuple[int, int]:
    if not isinstance(value, list) or len(value) != 2:
        fail("frameRange must be [start, end]")
    start, end = value
    if not isinstance(start, int) or not isinstance(end, int) or start <= 0 or end < start:
        fail(f"invalid frameRange: {value}")
    return start - 1, end - 1


def extract_clip(source_path: Path, output_path: Path, start: int, end: int) -> None:
    with Image.open(source_path) as image:
        frames = []
        durations = []
        for index, frame in enumerate(ImageSequence.Iterator(image)):
            if index < start:
                continue
            if index > end:
                break
            frames.append(frame.convert("RGBA"))
            durations.append(frame.info.get("duration", image.info.get("duration", 100)))

        if not frames:
            fail(f"frameRange [{start + 1}, {end + 1}] selected no frames in {source_path}")
        if len(frames) != end - start + 1:
            fail(f"frameRange [{start + 1}, {end + 1}] exceeds frame count in {source_path}")

        output_path.parent.mkdir(parents=True, exist_ok=True)
        frames[0].save(
            output_path,
            save_all=True,
            append_images=frames[1:],
            duration=durations,
            loop=0,
            disposal=2,
        )


def write_generated_qrc(skin_root: Path, clip_ids: list[str]) -> Path:
    qrc_path = skin_root / "generated" / "clips.qrc"
    qrc_path.parent.mkdir(parents=True, exist_ok=True)
    skin_id = skin_root.name
    lines = [
        "<RCC>",
        f'  <qresource prefix="/skins/{html.escape(skin_id)}/generated/clips">',
    ]
    for clip_id in sorted(clip_ids):
        escaped = html.escape(clip_id)
        lines.append(f'    <file alias="{escaped}.gif">clips/{escaped}.gif</file>')
    lines.extend([
        "  </qresource>",
        "</RCC>",
        "",
    ])
    qrc_path.write_text("\n".join(lines), encoding="utf-8")
    return qrc_path


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        fail("usage: split_manifest_clips.py <skin-root>")

    skin_root = Path(argv[1])
    manifest = load_manifest(skin_root)
    clips = manifest.get("clips", {})
    if not isinstance(clips, dict):
        fail("manifest clips must be an object")

    output_dir = skin_root / "generated" / "clips"
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    generated_clip_ids = []
    for clip_id, definition in clips.items():
        if not isinstance(definition, dict):
            fail(f"clip definition must be an object: {clip_id}")
        source_path = source_path_for_url(skin_root, definition.get("source", ""))
        start, end = frame_range_from_manifest(definition.get("frameRange"))
        output_path = output_path_for_clip_id(output_dir, clip_id)
        extract_clip(source_path, output_path, start, end)
        generated_clip_ids.append(clip_id)

    qrc_path = write_generated_qrc(skin_root, generated_clip_ids)
    print(f"generated {len(generated_clip_ids)} clip(s) into {output_dir}")
    print(f"generated clip qrc: {qrc_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
```

- [ ] **Step 5: Ignore generated clips**

Append to `.gitignore`:

```gitignore

# Generated skin clip assets
apps/desktop/resources/skins/*/generated/
```

- [ ] **Step 6: Run the script test to verify it passes**

Run:

```bash
python3 -m pip install -r tools/requirements.txt
python3 tests/test_split_manifest_clips.py
```

Expected: PASS with `phase 2.4.2 clip slicing tool ok`.

- [ ] **Step 7: Commit**

```bash
git add .gitignore tools/requirements.txt tools/split_manifest_clips.py tests/test_split_manifest_clips.py
git commit -m "feat: 添加 GIF 片段预切分脚本"
```

## Task 2: Loader Schema v4 Clip Resolution

**Files:**

- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.h`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`

- [ ] **Step 1: Write the failing loader smoke**

In `apps/desktop/tests/skin_manifest_loader_smoke.cpp`, replace the `skin:` URL assertions near the top with:

```cpp
    require(
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("file:assets/body/idle.gif"), rootUrl).toString()
            == QStringLiteral("file:///tmp/example-skin/assets/body/idle.gif"),
        "file: URL should resolve under file root"
    );
    require(
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("qrc:/pet/stand-right.gif"), rootUrl).toString()
            == QStringLiteral("qrc:/pet/stand-right.gif"),
        "absolute qrc URL should stay unchanged"
    );
    require(
        !SkinManifestLoader::resolveSkinUrl(QStringLiteral("file:../escape.gif"), rootUrl).isValid(),
        "file: URL must reject parent traversal"
    );
```

In the temporary skin setup, create generated clips before writing `manifest.json`:

```cpp
    require(skinDir.mkpath(QStringLiteral("generated/clips")), "generated clips directory should be created");
    require(writeFile(skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif")),
                      QStringLiteral("fake gif placeholder")),
            "generated clip should be written");
```

Replace the test manifest's `clips` and `actions.objecting.variants.right` with schema v4:

```json
  "schemaVersion": 4,
  "clips": {
    "thinking.enter.right": {
      "source": "file:assets/body/idle/stand.gif",
      "frameRange": [1, 4]
    }
  },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "clip": "file:assets/body/idle/stand.gif"
        }
      }
    },
    "objecting": {
      "loopMode": "onceThenHold",
      "variants": {
        "right": {
          "clip": "thinking.enter.right"
        }
      }
    }
  }
```

Replace the frame-range assertions with generated URL assertions:

```cpp
    const QString expectedGeneratedClipUrl = QUrl::fromLocalFile(
        skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif"))
    ).toString();
    require(objectingVariant.url.toString() == expectedGeneratedClipUrl,
            "bare clip variant should resolve to generated/clips/{clip}.gif");
```

Add a missing generated clip rejection check after loading the valid filesystem manifest:

```cpp
    QFile::remove(skinDir.filePath(QStringLiteral("generated/clips/thinking.enter.right.gif")));
    SkinManifest missingGeneratedClip = SkinManifestLoader::loadFromDirectory(dir.path());
    require(missingGeneratedClip.actions.isEmpty(),
            "loader should reject manifests whose generated clip files are missing");
```

- [ ] **Step 2: Run loader smoke to verify it fails**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build --output-on-failure -R skin_manifest_loader_smoke
```

Expected: FAIL because `file:` URLs are not resolved and `clips.source` is not parsed.

- [ ] **Step 3: Update manifest data structures**

In `apps/desktop/src/pet/manifest/SkinManifest.h`, replace `AnimationVariant` and `ClipDefinition` with:

```cpp
struct AnimationVariant
{
    QUrl url;
};

struct ClipDefinition
{
    QUrl sourceUrl;
    QUrl generatedUrl;
    int sourceFrameStart = -1;
    int sourceFrameEnd = -1;
};
```

Add schema version to `SkinManifest`:

```cpp
    int schemaVersion = 4;
```

- [ ] **Step 4: Update `resolveSkinUrl` for `file:`**

In `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`, replace `resolveSkinUrl` with:

```cpp
QUrl SkinManifestLoader::resolveSkinUrl(const QString &rawUrl, const QUrl &rootUrl)
{
    const QString trimmed = rawUrl.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (!trimmed.startsWith(QStringLiteral("file:"))) {
        return QUrl(trimmed);
    }

    QString relativePath = trimmed.mid(QStringLiteral("file:").size());
    while (relativePath.startsWith(QLatin1Char('/'))) {
        relativePath.remove(0, 1);
    }
    if (relativePath.isEmpty() || relativePath.contains(QStringLiteral(".."))) {
        return {};
    }

    QUrl resolved = rootUrl;
    QString base = resolved.path();
    if (!base.endsWith(QLatin1Char('/'))) {
        base.append(QLatin1Char('/'));
    }
    resolved.setPath(base + relativePath);
    return resolved;
}
```

- [ ] **Step 5: Add generated clip existence helper**

Add this helper in the anonymous namespace in `SkinManifestLoader.cpp`:

```cpp
bool resolvedUrlExists(const QUrl &url)
{
    if (url.isEmpty()) {
        return false;
    }
    if (url.scheme() == QStringLiteral("qrc")) {
        return QFile::exists(QStringLiteral(":") + url.path());
    }
    if (url.isLocalFile()) {
        return QFileInfo::exists(url.toLocalFile());
    }
    return true;
}

QUrl generatedClipUrlForId(const QString &clipId, const QUrl &rootUrl)
{
    if (clipId.isEmpty()
            || clipId.contains(QLatin1Char('/'))
            || clipId.contains(QLatin1Char('\\'))
            || clipId.contains(QStringLiteral(".."))) {
        return {};
    }
    return SkinManifestLoader::resolveSkinUrl(
        QStringLiteral("file:generated/clips/%1.gif").arg(clipId),
        rootUrl
    );
}
```

- [ ] **Step 6: Parse schema v4 clips and variants**

In `parseManifestDocument`, set:

```cpp
    manifest.schemaVersion = root.value("schemaVersion").toInt(4);
```

Replace clip parsing with:

```cpp
    bool invalidClipDefinition = false;
    bool missingGeneratedClip = false;
    const QJsonObject clips = root.value(QStringLiteral("clips")).toObject();
    for (auto it = clips.constBegin(); it != clips.constEnd(); ++it) {
        const QJsonObject clipObject = it.value().toObject();
        ClipDefinition clip;
        clip.sourceUrl = SkinManifestLoader::resolveSkinUrl(
            clipObject.value(QStringLiteral("source")).toString(),
            manifest.skinRootUrl
        );
        clip.generatedUrl = generatedClipUrlForId(it.key(), manifest.skinRootUrl);
        const auto frames = frameRangeFromJson(clipObject.value(QStringLiteral("frameRange")).toArray());
        clip.sourceFrameStart = frames.first;
        clip.sourceFrameEnd = frames.second;
        const bool completeDefinition = !it.key().isEmpty()
                && !clip.sourceUrl.isEmpty()
                && !clip.generatedUrl.isEmpty()
                && clip.sourceFrameStart >= 0
                && clip.sourceFrameEnd >= clip.sourceFrameStart;
        if (!completeDefinition) {
            invalidClipDefinition = true;
            continue;
        }
        if (!resolvedUrlExists(clip.generatedUrl)) {
            missingGeneratedClip = true;
            continue;
        }
        manifest.clips.insert(it.key(), clip);
    }
    if (invalidClipDefinition || missingGeneratedClip) {
        return {};
    }
```

Replace `parseAnimationVariant` with:

```cpp
AnimationVariant parseAnimationVariant(
    const QJsonObject &object,
    const SkinManifest &manifest,
    const QUrl &skinRootUrl
)
{
    AnimationVariant variant;

    const QString clipRef = object.value(QStringLiteral("clip")).toString().trimmed();
    if (clipRef.startsWith(QStringLiteral("file:"))) {
        variant.url = SkinManifestLoader::resolveSkinUrl(clipRef, skinRootUrl);
        return variant;
    }

    if (!clipRef.isEmpty() && manifest.clips.contains(clipRef)) {
        variant.url = manifest.clips.value(clipRef).generatedUrl;
        return variant;
    }

    const QString legacyAnimation = object.value(QStringLiteral("animation")).toString().trimmed();
    if (!legacyAnimation.isEmpty()) {
        variant.url = SkinManifestLoader::resolveSkinUrl(legacyAnimation, skinRootUrl);
    }

    return variant;
}
```

- [ ] **Step 7: Run loader smoke to verify it passes**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build --output-on-failure -R skin_manifest_loader_smoke
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add apps/desktop/src/pet/manifest/SkinManifest.h apps/desktop/src/pet/manifest/SkinManifestLoader.h apps/desktop/src/pet/manifest/SkinManifestLoader.cpp apps/desktop/tests/skin_manifest_loader_smoke.cpp
git commit -m "feat: 支持 schema v4 clip 解析"
```

## Task 3: Runtime AnimationPool Rename

**Files:**

- Rename: `apps/desktop/src/pet/selection/ActionPoolSelector.h`
- Rename: `apps/desktop/src/pet/selection/ActionPoolSelector.cpp`
- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/requests/ActionRequest.h`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/src/pet/selection/ExpressionMappingResolver.cpp`
- Modify: `apps/desktop/src/pet/interaction/InteractionPipeline.cpp`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`

- [ ] **Step 1: Write failing tests for `animationPools`**

In `apps/desktop/tests/skin_manifest_loader_smoke.cpp`, add an `animationPools` block to the temp manifest:

```json
  "animationPools": {
    "click.fallback": {
      "entries": [
        { "command": "returnToIdle", "weight": 1 }
      ]
    }
  }
```

Also add `pool.dispatch` to the existing `recipes` block so the loader test covers JSON `"pool"` routing:

```json
  "pool.dispatch": {
    "steps": [
      { "pool": "click.fallback" }
    ]
  }
```

After loading the manifest, add:

```cpp
    require(manifest.animationPools.contains(QStringLiteral("click.fallback")),
            "loader should parse animationPools");
    const RecipeDefinition poolRecipe = manifest.recipes.value(QStringLiteral("pool.dispatch"));
    require(poolRecipe.steps.size() == 1,
            "loader should parse recipe step that dispatches to a pool");
    require(poolRecipe.steps.first().request.kind == ActionRequestKind::AnimationPool,
            "JSON pool key should route to AnimationPool request kind");
    require(poolRecipe.steps.first().request.targetId == QStringLiteral("click.fallback"),
            "JSON pool key should preserve target pool id");
```

In `apps/desktop/tests/pet_runtime_smoke.cpp`, replace:

```cpp
    require(runtime.manifest().actionPools.contains("click.fallback"), "manifest 应加载 click.fallback 动作池");
```

with:

```cpp
    require(runtime.manifest().animationPools.contains("click.fallback"), "manifest 应加载 click.fallback 动画池");
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R 'skin_manifest_loader_smoke|pet_runtime_smoke'
```

Expected: FAIL because `animationPools` and `AnimationPoolSelector` do not exist.

- [ ] **Step 3: Rename selector files**

Run:

```bash
git mv apps/desktop/src/pet/selection/ActionPoolSelector.h apps/desktop/src/pet/selection/AnimationPoolSelector.h
git mv apps/desktop/src/pet/selection/ActionPoolSelector.cpp apps/desktop/src/pet/selection/AnimationPoolSelector.cpp
```

Update include guards/content by replacing:

```cpp
#include "pet/selection/ActionPoolSelector.h"
class ActionPoolSelector
```

with:

```cpp
#include "pet/selection/AnimationPoolSelector.h"
class AnimationPoolSelector
```

- [ ] **Step 4: Rename manifest pool types**

In `apps/desktop/src/pet/manifest/SkinManifest.h`, rename:

```cpp
struct ActionPoolEntry
struct ActionPoolDefinition
QHash<QString, ActionPoolDefinition> actionPools;
```

to:

```cpp
struct AnimationPoolEntry
struct AnimationPoolDefinition
QHash<QString, AnimationPoolDefinition> animationPools;
```

Keep the JSON request key `"pool"` unchanged.

- [ ] **Step 5: Rename ActionRequest kind and factory**

In `apps/desktop/src/pet/requests/ActionRequest.h`, replace `ActionPool` with `AnimationPool`:

```cpp
enum class ActionRequestKind
{
    None,
    AnimationPool,
    Recipe,
    Action,
    ReturnToIdle,
    ToggleFacing,
    SpawnProp,
    PlaySound,
};

static ActionRequest animationPool(const QString &poolId)
{
    ActionRequest request;
    request.kind = ActionRequestKind::AnimationPool;
    request.targetId = poolId.trimmed();
    return request;
}
```

- [ ] **Step 6: Update loader and runtime references**

In `SkinManifestLoader.cpp`, update `requestFromJsonObject`:

```cpp
    if (object.contains("pool")) {
        return ActionRequest::animationPool(object.value("pool").toString());
    }
```

Also support command-key dispatch:

```cpp
    const QString command = object.value("command").toString();
    if (command == QStringLiteral("returnToIdle")) {
        return ActionRequest::returnToIdle();
    }
    if (command == QStringLiteral("toggleFacing")) {
        return ActionRequest::toggleFacing();
    }
```

Replace parsing of `actionPools` with `animationPools`:

```cpp
    const QJsonObject animationPools = root.value("animationPools").toObject();
    for (auto it = animationPools.constBegin(); it != animationPools.constEnd(); ++it) {
        const QJsonObject poolObject = it.value().toObject();

        AnimationPoolDefinition pool;
        pool.label = poolObject.value("label").toString(it.key());
        ...
        AnimationPoolEntry entry;
        ...
        manifest.animationPools.insert(it.key(), pool);
    }
```

In runtime and resolver files, replace:

```cpp
ActionPoolSelector
ActionPoolEntry
ActionPoolDefinition
ActionRequestKind::ActionPool
actionPools
playActionFromPool
```

with:

```cpp
AnimationPoolSelector
AnimationPoolEntry
AnimationPoolDefinition
ActionRequestKind::AnimationPool
animationPools
playAnimationFromPool
```

- [ ] **Step 7: Update CMake source lists**

In `apps/desktop/CMakeLists.txt`, replace every selector file reference:

```cmake
src/pet/selection/ActionPoolSelector.cpp
src/pet/selection/ActionPoolSelector.h
```

with:

```cmake
src/pet/selection/AnimationPoolSelector.cpp
src/pet/selection/AnimationPoolSelector.h
```

- [ ] **Step 8: Run tests to verify rename passes**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke PetRuntimeSmoke ChatControllerSmoke SettingsServiceSmoke
ctest --test-dir build --output-on-failure -R 'skin_manifest_loader_smoke|pet_runtime_smoke|chat_controller_smoke|settings_service_smoke'
```

Expected: PASS.

- [ ] **Step 9: Commit**

```bash
git add apps/desktop/src/pet apps/desktop/CMakeLists.txt apps/desktop/tests/skin_manifest_loader_smoke.cpp apps/desktop/tests/pet_runtime_smoke.cpp
git commit -m "refactor: 统一动画池命名"
```

## Task 4: Remove Runtime FrameRange Playback

**Files:**

- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.h`
- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`

- [ ] **Step 1: Write the failing runtime assertions**

In `apps/desktop/tests/pet_runtime_smoke.cpp`, replace assertions that check `currentFrameStart()` / `currentFrameEnd()` for `talking` with URL checks:

```cpp
    require(runtime.currentPhaseId() == "enter",
            "talking should start at enter phase");
    require(runtime.currentAnimationUrl().toString().endsWith("generated/clips/talking.enter.right.gif"),
            "talking enter should use pre-cut generated GIF");
    runtime.handleAnimationFinished();
    require(runtime.currentPhaseId() == "loop",
            "talking enter should advance to loop");
    require(runtime.currentAnimationUrl().toString().endsWith("generated/clips/talking.loop.right.gif"),
            "talking loop should use pre-cut generated GIF");
```

Delete the temporary `frameRangeManifest` setup block that mutates `rangedObjectingRight.frameStart` and `frameEnd`. Replace it with:

```cpp
    const QUrl objectingUrlBeforeReload = runtime.currentAnimationUrl();
    const int objectingSerialBeforeReload = runtime.playbackSerial();
    require(runtime.reloadActiveSkinPreservingPlayback(),
            "preserve reload should keep current objecting action while refreshing manifest metadata");
    require(runtime.currentActionId() == "objecting",
            "preserve reload should keep the current action when it still exists");
    require(runtime.currentAnimationUrl() == objectingUrlBeforeReload,
            "preserve reload should recompute the same complete GIF URL from the reloaded manifest");
    require(runtime.playbackSerial() == objectingSerialBeforeReload,
            "preserve reload should not restart playback when the complete GIF URL is unchanged");
```

- [ ] **Step 2: Run runtime smoke to verify it fails**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R pet_runtime_smoke
```

Expected: FAIL because generated clip URLs are not yet in Miles manifest and runtime still exposes frame range properties.

- [ ] **Step 3: Remove frame range from runtime API**

In `apps/desktop/src/pet/PetRuntime.h`, delete:

```cpp
Q_PROPERTY(int currentFrameStart READ currentFrameStart NOTIFY currentAnimationUrlChanged)
Q_PROPERTY(int currentFrameEnd READ currentFrameEnd NOTIFY currentAnimationUrlChanged)
int currentFrameStart() const { return m_currentFrameStart; }
int currentFrameEnd() const { return m_currentFrameEnd; }
int m_currentFrameStart = -1;
int m_currentFrameEnd = -1;
```

In `apps/desktop/src/pet/PetRuntime.cpp`, delete all `nextFrameStart`, `nextFrameEnd`, `m_currentFrameStart`, and `m_currentFrameEnd` comparisons in `setCurrentPhase`. `animationChanged` should only compare URLs:

```cpp
    const bool animationChanged = (m_currentAnimationUrl != nextAnimationUrl);
```

- [ ] **Step 4: Remove manual frame playback from surface**

In `apps/desktop/src/pet/surface/PetSurfaceWindow.h`, delete:

```cpp
bool loadManualFrameRange(const QString &path);
void stopManualFrameRange();
void showManualFrame(int playbackSerial);
void advanceManualFrame(int playbackSerial);
void completeManualFrameRangeLoop(int playbackSerial);
int currentEffectiveEndFrame();
bool jumpToFrameStartNowIfNeeded();
QTimer m_manualFrameTimer;
QVector<QPixmap> m_manualFrames;
QVector<int> m_manualFrameDelays;
int m_manualFrameIndex = 0;
bool m_manualFramePlayback = false;
int m_lastInvalidFrameRangeWarningSerial = -1;
```

In `PetSurfaceWindow.cpp`, delete:

```cpp
#include <QImageReader>
#include <QPixmap>
#include <QVector>
```

Delete the manual frame timer connection from the constructor:

```cpp
    m_manualFrameTimer.setSingleShot(true);
    connect(&m_manualFrameTimer, &QTimer::timeout, this, [this]() {
        advanceManualFrame(m_runtime->playbackSerial());
    });
```

In `restartMovieFromRuntime`, replace the function body with the QMovie-only path:

```cpp
void PetSurfaceWindow::restartMovieFromRuntime()
{
    const QString path = imagePathFromUrl(m_runtime->currentAnimationUrl());
    if (path.isEmpty()) {
        clearMask();
        return;
    }

    m_petLabel->setMovie(m_movie);
    m_movie->stop();
    m_movie->setFileName(path);
    m_movie->start();
    applyCurrentFrameMask();
}
```

In `handleMovieFrameChanged`, replace:

```cpp
    const int endFrame = currentEffectiveEndFrame();
```

with:

```cpp
    const int endFrame = frameCount > 0 ? frameCount - 1 : -1;
```

Delete the frameStart loop wrapping block:

```cpp
    if (frameCount > 0
            && endFrame >= 0
            && m_runtime->currentFrameStart() >= 0
            && m_runtime->currentLoopMode() == QStringLiteral("loop")
            && frame > endFrame) {
        ...
    }
```

Delete implementations of `loadManualFrameRange`, `stopManualFrameRange`, `showManualFrame`, `advanceManualFrame`, `completeManualFrameRangeLoop`, `currentEffectiveEndFrame`, and `jumpToFrameStartNowIfNeeded`.

- [ ] **Step 5: Run runtime smoke to verify API cleanup compiles**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R pet_runtime_smoke
```

Expected: It may still FAIL on generated URLs until Task 5 migrates Miles manifest, but it should compile without `currentFrameStart` / `currentFrameEnd`.

- [ ] **Step 6: Commit**

```bash
git add apps/desktop/src/pet/PetRuntime.h apps/desktop/src/pet/PetRuntime.cpp apps/desktop/src/pet/surface/PetSurfaceWindow.h apps/desktop/src/pet/surface/PetSurfaceWindow.cpp apps/desktop/tests/pet_runtime_smoke.cpp
git commit -m "refactor: 移除运行时帧段播放路径"
```

## Task 5: Miles Manifest v4 And Build Integration

**Files:**

- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`

- [ ] **Step 1: Update Miles manifest schema header and resource URLs**

In `apps/desktop/resources/skins/miles-edgeworth/manifest.json`:

- Change `"schemaVersion": 3` to `"schemaVersion": 4`.
- Replace every `skin:assets/` with `file:assets/`.
- Rename top-level `"actionPools"` to `"animationPools"`.
- Replace every action variant key `"animation"` with `"clip"` when the value points to a complete file.

Run:

```bash
rg -n '"animation"|"actionPools"|skin:' apps/desktop/resources/skins/miles-edgeworth/manifest.json
```

Expected: no output.

- [ ] **Step 2: Add Miles top-level clips**

Add this top-level `clips` object before `"actions"`:

```json
  "clips": {
    "thinking.enter.right": { "source": "file:assets/body/gestures/thinking-right.gif", "frameRange": [1, 4] },
    "thinking.enter.left": { "source": "file:assets/body/gestures/thinking-left.gif", "frameRange": [1, 4] },
    "thinking.loop.right": { "source": "file:assets/body/gestures/thinking-right.gif", "frameRange": [5, 8] },
    "thinking.loop.left": { "source": "file:assets/body/gestures/thinking-left.gif", "frameRange": [5, 8] },
    "thinking.exit.right": { "source": "file:assets/body/gestures/thinking-right.gif", "frameRange": [44, 47] },
    "thinking.exit.left": { "source": "file:assets/body/gestures/thinking-left.gif", "frameRange": [44, 47] },
    "talking.enter.right": { "source": "file:assets/body/interaction/crossed-right.gif", "frameRange": [1, 4] },
    "talking.enter.left": { "source": "file:assets/body/interaction/crossed-left.gif", "frameRange": [1, 4] },
    "talking.loop.right": { "source": "file:assets/body/interaction/crossed-right.gif", "frameRange": [5, 8] },
    "talking.loop.left": { "source": "file:assets/body/interaction/crossed-left.gif", "frameRange": [5, 8] },
    "talking.exit.right": { "source": "file:assets/body/interaction/crossed-right.gif", "frameRange": [9, 11] },
    "talking.exit.left": { "source": "file:assets/body/interaction/crossed-left.gif", "frameRange": [9, 11] }
  },
```

- [ ] **Step 3: Replace thinking/talking phase variants**

In `actions.thinking.phases`, replace each variant with:

```json
"enter": {
  "loopMode": "once",
  "variants": {
    "right": { "clip": "thinking.enter.right" },
    "left": { "clip": "thinking.enter.left" }
  }
},
"loop": {
  "loopMode": "loop",
  "variants": {
    "right": { "clip": "thinking.loop.right" },
    "left": { "clip": "thinking.loop.left" }
  }
},
"exit": {
  "loopMode": "onceThenHold",
  "variants": {
    "right": { "clip": "thinking.exit.right" },
    "left": { "clip": "thinking.exit.left" }
  }
}
```

In `actions.talking.phases`, replace each variant with:

```json
"enter": {
  "loopMode": "once",
  "nextPhase": "loop",
  "variants": {
    "right": { "clip": "talking.enter.right" },
    "left": { "clip": "talking.enter.left" }
  }
},
"loop": {
  "loopMode": "loop",
  "variants": {
    "right": { "clip": "talking.loop.right" },
    "left": { "clip": "talking.loop.left" }
  }
},
"exit": {
  "loopMode": "onceThenHold",
  "variants": {
    "right": { "clip": "talking.exit.right" },
    "left": { "clip": "talking.exit.left" }
  }
}
```

Run:

```bash
rg -n 'frameRange' apps/desktop/resources/skins/miles-edgeworth/manifest.json
```

Expected: only the top-level `clips` section contains `frameRange`.

- [ ] **Step 4: Confirm sleep uses complete GIF files**

Run:

```bash
python3 - <<'PY'
import json
from pathlib import Path
manifest = json.loads(Path("apps/desktop/resources/skins/miles-edgeworth/manifest.json").read_text(encoding="utf-8"))
sleep_text = json.dumps(manifest["actions"]["sleep"], ensure_ascii=False)
if "frameRange" in sleep_text:
    raise SystemExit("sleep action still uses frameRange")
print("sleep action uses complete GIF files")
PY
```

Expected: PASS with `sleep action uses complete GIF files`.

- [ ] **Step 5: Add CMake generation target**

Near the top of `apps/desktop/CMakeLists.txt`, after `find_package(Qt6 ...)`, add:

```cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(MILES_SKIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/resources/skins/miles-edgeworth")
set(MILES_GENERATED_CLIPS_QRC "${MILES_SKIN_ROOT}/generated/clips.qrc")
set_source_files_properties("${MILES_GENERATED_CLIPS_QRC}" PROPERTIES GENERATED TRUE)

add_custom_target(GenerateMilesClips
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/split_manifest_clips.py"
            "${MILES_SKIN_ROOT}"
    BYPRODUCTS
            "${MILES_GENERATED_CLIPS_QRC}"
    DEPENDS
            "${CMAKE_SOURCE_DIR}/tools/split_manifest_clips.py"
            "${MILES_SKIN_ROOT}/manifest.json"
    COMMENT "Generating pre-cut Miles GIF clips"
    VERBATIM
)

function(add_miles_generated_clips_qrc target_name)
    target_sources(${target_name} PRIVATE "${MILES_GENERATED_CLIPS_QRC}")
    add_dependencies(${target_name} GenerateMilesClips)
    set_property(
        TARGET ${target_name}
        APPEND PROPERTY AUTOGEN_TARGET_DEPENDS "${MILES_GENERATED_CLIPS_QRC}"
    )
endfunction()
```

After each target that includes `resources/pet_assets.qrc`, call the helper. At minimum:

```cmake
add_miles_generated_clips_qrc(MilesEdgeworthDesktop)
add_miles_generated_clips_qrc(SkinManifestLoaderSmoke)
add_miles_generated_clips_qrc(PetRuntimeSmoke)
add_miles_generated_clips_qrc(ChatControllerSmoke)
add_miles_generated_clips_qrc(SettingsServiceSmoke)
```

This intentionally avoids a hard-coded `MILES_GENERATED_CLIPS` list. Adding a future clip should only require editing `manifest.json`; the generator rewrites `generated/clips.qrc`, and AUTORCC packages whatever the manifest declares.

- [ ] **Step 6: Generate clips and run tests**

Run:

```bash
python3 tools/split_manifest_clips.py apps/desktop/resources/skins/miles-edgeworth
cmake --build build --target GenerateMilesClips SkinManifestLoaderSmoke PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R 'skin_manifest_loader_smoke|pet_runtime_smoke'
```

Expected: PASS. `git status --short` should show generated files ignored, not untracked.

- [ ] **Step 7: Commit**

```bash
git add apps/desktop/resources/skins/miles-edgeworth/manifest.json apps/desktop/CMakeLists.txt apps/desktop/tests/pet_runtime_smoke.cpp apps/desktop/tests/skin_manifest_loader_smoke.cpp
git commit -m "feat: 迁移 Miles 到预切片 manifest"
```

## Task 6: Static Contract Check

**Files:**

- Create: `tests/check_phase_2_4_2_precut_clips.py`
- Modify: `README.md`

- [ ] **Step 1: Write the static contract check**

Create `tests/check_phase_2_4_2_precut_clips.py`:

```python
#!/usr/bin/env python3
import json
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


manifest_path = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"
skin_root = manifest_path.parent
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
manifest_text = manifest_path.read_text(encoding="utf-8")
surface_h = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h")
surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
cmake = read("apps/desktop/CMakeLists.txt")

require(manifest.get("schemaVersion") == 4, "Miles manifest must use schemaVersion 4")
require("skin:" not in manifest_text, "Miles manifest must not use skin: URLs")
require('"actionPools"' not in manifest_text, "Miles manifest must not use actionPools")
require('"animationPools"' in manifest_text, "Miles manifest must use animationPools")
require('"animation"' not in manifest_text, "Miles action variants must not use animation")

clips = manifest.get("clips", {})
require(isinstance(clips, dict) and clips, "Miles manifest must define top-level clips")
required_clips = {
    "thinking.enter.right",
    "thinking.enter.left",
    "thinking.loop.right",
    "thinking.loop.left",
    "thinking.exit.right",
    "thinking.exit.left",
    "talking.enter.right",
    "talking.enter.left",
    "talking.loop.right",
    "talking.loop.left",
    "talking.exit.right",
    "talking.exit.left",
}
require(required_clips.issubset(clips.keys()), "Miles manifest missing expected thinking/talking clips")

actions_text = json.dumps(manifest.get("actions", {}), ensure_ascii=False)
require("frameRange" not in actions_text, "frameRange must only appear in top-level clips")

for forbidden in [
    "loadManualFrameRange",
    "m_manualFrames",
    "m_manualFramePlayback",
    "QImageReader",
    "setPixmap(m_manualFrames",
]:
    require(forbidden not in surface_h + surface_cpp, f"runtime manual frame playback remains: {forbidden}")

for forbidden in ["currentFrameStart", "currentFrameEnd", "m_currentFrameStart", "m_currentFrameEnd"]:
    require(forbidden not in runtime_h, f"PetRuntime must not expose runtime frameRange state: {forbidden}")

require("frameStart" not in manifest_h, "AnimationVariant must not carry runtime frameStart")
require("frameEnd" not in manifest_h, "AnimationVariant must not carry runtime frameEnd")
require("sourceFrameStart" in manifest_h, "ClipDefinition should keep source frame metadata for tooling/debug")
require("sourceFrameEnd" in manifest_h, "ClipDefinition should keep source frame metadata for tooling/debug")
require("generatedClipUrlForId" in loader_cpp, "loader must resolve bare clip names to generated/clips")
require("resolvedUrlExists" in loader_cpp, "loader must reject missing generated clips")
require("GenerateMilesClips" in cmake, "CMake must generate Miles clips before qrc build")
require("add_miles_generated_clips_qrc" in cmake, "CMake must attach generated clip qrc to qrc targets")

result = subprocess.run(
    [sys.executable, str(ROOT / "tools/split_manifest_clips.py"), str(skin_root)],
    cwd=ROOT,
    text=True,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
)
require(result.returncode == 0, result.stderr)

generated_dir = skin_root / "generated" / "clips"
generated_files = sorted(generated_dir.glob("*.gif"))
require(len(generated_files) == len(clips), "generated clip file count must match manifest clips count")
for clip in clips:
    require((generated_dir / f"{clip}.gif").is_file(), f"generated clip missing: {clip}")

generated_qrc = skin_root / "generated" / "clips.qrc"
require(generated_qrc.is_file(), "generated clips.qrc must exist")

tree = ET.parse(generated_qrc)
aliases = {
    file_node.attrib.get("alias", "")
    for resource in tree.getroot().findall("qresource")
    if resource.attrib.get("prefix") == "/skins/miles-edgeworth/generated/clips"
    for file_node in resource.findall("file")
}
require(aliases == {f"{clip}.gif" for clip in clips}, "generated qrc aliases must match manifest clips")

print("phase 2.4.2 precut clips contract ok")
```

- [ ] **Step 2: Run the static check**

Run:

```bash
python3 tests/check_phase_2_4_2_precut_clips.py
```

Expected: PASS with `phase 2.4.2 precut clips contract ok`.

- [ ] **Step 3: Update README verification commands**

In `README.md`, add the Phase 2.4.2 check under "常用验证命令":

```bash
python3 tests/check_phase_2_4_2_precut_clips.py
python3 tests/test_split_manifest_clips.py
```

Also update the current-status bullet that mentions `skin:assets/...` to `file:assets/...` and schema v4.

- [ ] **Step 4: Run final verification**

Run:

```bash
python3 tests/test_split_manifest_clips.py
python3 tests/check_phase_2_4_2_precut_clips.py
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
```

Expected:

- Python checks PASS.
- CMake build succeeds.
- All CTest tests pass.
- `git diff --check` prints no whitespace errors.

- [ ] **Step 5: Commit**

```bash
git add tests/check_phase_2_4_2_precut_clips.py README.md
git commit -m "test: 增加预切片契约检查"
```

## Implementation Notes

- Do not commit generated GIF files. They must be regenerated by `tools/split_manifest_clips.py`.
- Keep JSON request key `"pool"` even after C++ renames to `AnimationPool`; this keeps manifest request objects concise and matches the design docs.
- Do not reintroduce runtime `frameRange` fallback. If a generated clip is missing, loader rejects the manifest so the error is visible.
- `QImageReader` may remain in unrelated alpha-mask code such as `WindowInputMaskController`; the contract only bans it from `PetSurfaceWindow` manual animation playback.
- If CMake cannot find Pillow during local build, install it with `python3 -m pip install -r tools/requirements.txt`.
- To add a future clip after this phase: add one top-level `clips.<id>` definition with `source` and `frameRange`, reference that id through a variant `"clip": "<id>"`, then run `python3 tools/split_manifest_clips.py apps/desktop/resources/skins/miles-edgeworth` or just build the CMake target. Do not edit C++ or `pet_assets.qrc` for ordinary clip additions.

## Self-Review

**Spec coverage:** Task 1 covers build script and deterministic clipping. Task 2 covers `file:` and generated clip loader behavior. Task 3 covers `animationPools` naming. Task 4 removes runtime frameRange playback. Task 5 migrates Miles manifest, generated qrc packaging, and CMake generation. Task 6 adds regression checks and README verification.

**Placeholder scan:** The plan intentionally contains no `TBD`, no "add appropriate tests", and no unspecified edge-case instructions. Each task has exact files, code snippets, commands, and expected results.

**Type consistency:** The plan consistently uses `AnimationPoolEntry`, `AnimationPoolDefinition`, `AnimationPoolSelector`, `ActionRequestKind::AnimationPool`, `manifest.animationPools`, top-level `clips[*].source`, and variant `clip`.
