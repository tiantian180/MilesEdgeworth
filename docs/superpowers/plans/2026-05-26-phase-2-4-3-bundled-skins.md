# Phase 2.4.3 Bundled Skins Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将官方 Miles 皮肤从 qrc 内嵌资源迁移到随包文件系统目录 `skins/miles-edgeworth/`，并让用户皮肤与随包皮肤走同一套 `skin.json + manifest.json + assets/ + generated/clips/` 加载流程。运行时不再依赖 qrc 皮肤资源；qrc 只保留 app icon 等非皮肤资源。

**Architecture:** `SkinManifestLoader` 只发现文件系统皮肤：用户目录 `<AppDataLocation>/skins/` 优先，应用目录 `<applicationDirPath()>/skins/` 兜底。`skin.json` 负责皮肤元数据与 `manifest.json` 定位，`manifest.json` 保持动画运行时 schema。皮肤资源引用只允许 `file:`，并解析到皮肤根目录内。官方 Miles 皮肤由 CMake 在构建时生成 `generated/clips/` 后复制到可执行文件同级的 `skins/miles-edgeworth/`。`PersonaStore` 按皮肤根目录判断写入位置：用户目录皮肤写回皮肤目录，应用目录皮肤写入 `<AppDataLocation>/skin-overrides/{skinId}/persona.md`。

**Tech Stack:** Qt 6 / C++17 / QML, CMake/Ninja, Python 3 静态检查与 GIF 切片脚本, CTest, `SkinManifestLoaderSmoke`, `PetRuntimeSmoke`, `SettingsServiceSmoke`。

---

## Ground Rules

- `docs/v2/设计方案/` 是真相源。实现遇到设计与代码冲突时先停下确认，不要用兼容 hack 绕过。
- 当前工作区已有用户改动，执行时只修改 Phase 2.4.3 需要的文件，不回滚用户文档改动。
- 不新增 qrc 皮肤兼容层。`skin:`、`qrc:/pet`、`qrc:/audio`、`qrc:/skins/miles-edgeworth` 在皮肤资源路径里都应被清掉。
- 不提交生成的 GIF。官方 Miles 的 `generated/clips/` 由 `tools/split_manifest_clips.py` 和 CMake 生成，再复制进构建产物。
- 每个任务完成后运行该任务列出的验证命令；通过后可以小步提交，commit message 用中文，不暴露 agent / codex 信息。

## Files To Touch

- `apps/desktop/resources/skins/miles-edgeworth/skin.json`
- `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- `apps/desktop/resources/pet_assets.qrc`
- `apps/desktop/CMakeLists.txt`
- `apps/desktop/src/pet/PetRuntime.cpp`
- `apps/desktop/src/pet/PetRuntimeSkin.cpp`
- `apps/desktop/src/pet/manifest/SkinDescriptor.h`
- `apps/desktop/src/pet/manifest/SkinManifest.h`
- `apps/desktop/src/pet/manifest/SkinManifestLoader.h`
- `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- `apps/desktop/src/pet/manifest/PersonaStore.h`
- `apps/desktop/src/pet/manifest/PersonaStore.cpp`
- `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
- `apps/desktop/tests/pet_runtime_smoke.cpp`
- `apps/desktop/tests/settings_service_smoke.cpp`
- `tools/split_manifest_clips.py`
- `tests/test_split_manifest_clips.py`
- `tests/check_phase_1_4_skin_package_contract.py`
- `tests/check_phase_1_4_skin_url_migration.py`
- `tests/check_phase_2_3_2_session_persona.py`
- `tests/check_phase_2_4_2_precut_clips.py`
- Legacy static checks currently asserting qrc skin resources:
  - `tests/check_phase_0_6_animation_runtime.py`
  - `tests/check_phase_0_7_phase_runtime.py`
  - `tests/check_phase_0_9_startup_idle_runtime.py`
  - `tests/check_phase_0_10_locomotion_runtime.py`
  - `tests/check_phase_0_11_hit_zone_runtime.py`
  - `tests/check_phase_0_12_double_click_sound_runtime.py`
  - `tests/check_phase_0_13_drag_shake_runtime.py`
  - `tests/check_phase_0_14_prosecutor_badge_runtime.py`
  - `tests/check_phase_0_15_menu_audio_movement_settings.py`
  - `tests/check_phase_0_16_menu_tea_sleep_wake.py`
  - `tests/check_phase_0_25_legacy_idle_once_pool.py`
  - `tests/check_phase_0_33_double_click_recipe_smoke.py`
  - `tests/check_phase_0_44_system_tray.py`
  - `tests/check_phase_0_56_pet_runtime_service_split.py`
  - `tests/check_phase_0_72_audio_capability.py`

## Task 1: Rewrite Static Contracts For Filesystem Skin Packages

**Purpose:** 先把长期契约测试改成 Phase 2.4.3 目标形态，确保旧 qrc 内嵌方案会失败。

- [ ] Update `tests/check_phase_1_4_skin_package_contract.py`.

Required assertions:

```python
skin_json = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/skin.json"))
manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
qrc = read("apps/desktop/resources/pet_assets.qrc")
loader_h = read("apps/desktop/src/pet/manifest/SkinManifestLoader.h")
loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
descriptor_h = read("apps/desktop/src/pet/manifest/SkinDescriptor.h")
cmake = read("apps/desktop/CMakeLists.txt")

require(skin_json.get("skinSchemaVersion") == 1, "skin.json must use skinSchemaVersion 1")
require(skin_json.get("id") == "miles-edgeworth", "official skin id must stay stable")
require(skin_json.get("manifest") == "manifest.json", "skin.json must point to manifest.json")
require(str(skin_json.get("thumbnail", "")).startswith("file:"), "thumbnail must use file: URL")
require(manifest.get("schemaVersion") == 4, "manifest.json must remain manifest schema v4")

for forbidden in [
    'prefix="/pet"',
    'prefix="/audio"',
    'prefix="/skins/miles-edgeworth"',
    "skins/miles-edgeworth",
    "stand-right.gif",
    "holdit0.wav",
]:
    require(forbidden not in qrc, f"pet_assets.qrc must not embed skin resource: {forbidden}")
require('prefix="/icon"' in qrc or "app-icon" in qrc, "pet_assets.qrc may keep app icon resources")

require("appSkinDirectoryPath" in loader_h, "loader must expose appSkinDirectoryPath()")
require("portableSkinDirectoryPath" not in loader_h + loader_cpp, "portableSkinDirectoryPath must be replaced")
require("loadFromResource" not in loader_h + loader_cpp, "qrc manifest loading entry must be removed")
require("SkinManifestLoader::discoverAll" in loader_cpp, "loader must still expose discoverAll")
require("qrc:/skins/miles-edgeworth" not in loader_cpp, "loader must not hardcode qrc built-in skin")
require("skinSchemaVersion" in descriptor_h + loader_cpp, "descriptor loading must validate skinSchemaVersion")
require("manifestVersion" not in descriptor_h + loader_cpp, "skin metadata must not use manifestVersion")
require("builtin" not in manifest_h + descriptor_h, "SkinManifest/SkinDescriptor must not expose builtin")

require("GenerateMilesClips" in cmake, "CMake must still generate Miles clips")
require("split_manifest_clips.py" in cmake, "CMake must call clip splitter")
require("copy_directory" in cmake and "skins/miles-edgeworth" in cmake, "CMake must copy official skin beside executable")
require("clips.qrc" not in cmake, "generated clips qrc must not be part of CMake")
```

- [ ] Update `tests/check_phase_1_4_skin_url_migration.py`.

Required behavior:

```python
for path in [
    ROOT / "apps/desktop/resources/skins/miles-edgeworth/skin.json",
    ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json",
]:
    raw = path.read_text(encoding="utf-8")
    for forbidden in ["skin:", "qrc:/pet/", "qrc:/audio/", "qrc:/skins/miles-edgeworth"]:
        require(forbidden not in raw, f"{path} must not contain {forbidden}")
```

- [ ] Update `tests/check_phase_2_4_2_precut_clips.py`.

Replace qrc assertions with filesystem generated clip assertions:

```python
generated_dir = SKIN_ROOT / "generated" / "clips"
require(generated_dir.exists(), "generated/clips directory must exist after running splitter")
actual = sorted(p.name for p in generated_dir.glob("*.gif"))
expected = sorted(f"{clip_id}.gif" for clip_id in manifest.get("clips", {}))
require(actual == expected, diff_message("generated clip files must exactly match manifest clips", expected, actual))
require(not (SKIN_ROOT / "generated" / "clips.qrc").exists(), "splitter must not create generated/clips.qrc")
```

- [ ] Update `tests/check_phase_2_3_2_session_persona.py`.

Required assertions:

```python
persona_store = read("apps/desktop/src/pet/manifest/PersonaStore.cpp")
require("skin-overrides" in persona_store, "PersonaStore must write app-dir overrides under skin-overrides")
require("persona-overrides" not in persona_store, "legacy persona-overrides path must be removed")
require("appSkinDirectoryPath" in persona_store, "PersonaStore must decide app-dir skin writability by appSkinDirectoryPath")
require("manifest.builtin" not in persona_store, "PersonaStore must not use builtin flag")
```

- [ ] Update legacy static checks listed in "Files To Touch".

Rules:

- qrc checks that only verified old `/pet` or `/audio` aliases should be deleted or replaced with manifest/file-system checks.
- checks that assert runtime sound URLs should no longer hardcode `qrc:/skins/miles-edgeworth`; they should assert the C++ smoke contains `milesSkinUrl(...)` or equivalent helper.
- `tests/check_phase_0_56_pet_runtime_service_split.py` should expect `SkinManifestLoader::loadFromDescriptor` / `discoverAll`, not `loadFromResource`.

- [ ] Run and confirm these fail before implementation:

```bash
python3 tests/check_phase_1_4_skin_package_contract.py
python3 tests/check_phase_1_4_skin_url_migration.py
python3 tests/check_phase_2_3_2_session_persona.py
python3 tests/check_phase_2_4_2_precut_clips.py
```

Expected before implementation: at least `check_phase_1_4_skin_package_contract.py` and `check_phase_2_4_2_precut_clips.py` fail because qrc built-in skin and `clips.qrc` still exist.

## Task 2: Stop Generating `clips.qrc`

**Purpose:** GIF 切片仍生成真实 `.gif`，但不再生成 qrc 索引，后续构建只复制文件系统皮肤目录。

- [ ] Update `tests/test_split_manifest_clips.py` first.

Required expectations:

```python
result = run_tool(skin)
require(result.returncode == 0, result.stderr)
generated = skin / "generated" / "clips"
require((generated / "thinking.enter.right.gif").exists(), "enter clip should be generated")
require((generated / "thinking.loop.right.gif").exists(), "loop clip should be generated")
require((generated / "thinking.exit.right.gif").exists(), "exit clip should be generated")
require(not (skin / "generated" / "clips.qrc").exists(), "tool must not write clips.qrc")
require("clips.qrc" not in result.stdout, "tool output must not reference clips.qrc")
```

- [ ] Update `tools/split_manifest_clips.py`.

Implementation shape:

```python
def generate_clips(skin_root: Path) -> tuple[Path, int]:
    manifest_path = skin_root / "manifest.json"
    manifest = load_manifest(manifest_path)
    clips = manifest.get("clips")
    if not isinstance(clips, dict):
        fail("manifest.json must contain object field: clips")

    generated_root = skin_root / "generated" / "clips"
    if generated_root.exists():
        shutil.rmtree(generated_root)
    generated_root.mkdir(parents=True, exist_ok=True)

    count = 0
    for clip_id, definition in sorted(clips.items()):
        output = generated_root / f"{clip_id}.gif"
        split_clip(skin_root, clip_id, definition, output)
        count += 1

    return generated_root, count

def main(argv: list[str]) -> int:
    if len(argv) != 2:
        fail("usage: split_manifest_clips.py <skin-root>")
    clips_root, count = generate_clips(Path(argv[1]).resolve())
    print(f"generated {count} clips under {clips_root}")
    return 0
```

Remove these concepts from the script:

- `write_qrc`
- `clips.qrc`
- `<qresource prefix="/skins/miles-edgeworth/generated/clips">`

- [ ] Run:

```bash
python3 tests/test_split_manifest_clips.py
python3 tools/split_manifest_clips.py apps/desktop/resources/skins/miles-edgeworth
```

Expected: tests pass; command prints generated clip count and leaves no `apps/desktop/resources/skins/miles-edgeworth/generated/clips.qrc`.

## Task 3: Make Loader Schema Filesystem-Only

**Purpose:** `SkinManifestLoader` 不再知道 qrc built-in skin。它只从目录发现 `skin.json`，解析 `file:` 资源，并加载 manifest。

- [ ] Update `apps/desktop/tests/skin_manifest_loader_smoke.cpp` first.

Required smoke coverage:

```cpp
const QUrl rootUrl = QUrl::fromLocalFile(tempSkinRoot.path() + QLatin1Char('/'));

require(SkinManifestLoader::resolveSkinUrl(QStringLiteral("file:assets/body/idle.gif"), rootUrl).isLocalFile(),
        "file: URL should resolve under filesystem skin root");
require(!SkinManifestLoader::resolveSkinUrl(QStringLiteral("skin:assets/body/idle.gif"), rootUrl).isValid(),
        "legacy skin: URL must be rejected");
require(!SkinManifestLoader::resolveSkinUrl(QStringLiteral("qrc:/pet/stand-right.gif"), rootUrl).isValid(),
        "qrc skin URL must be rejected");
require(!SkinManifestLoader::resolveSkinUrl(QStringLiteral("file:../escape.gif"), rootUrl).isValid(),
        "file: URL must not escape skin root");

writeText(tempSkinRoot.filePath("skin.json"), R"JSON({
  "skinSchemaVersion": 1,
  "id": "test-skin",
  "name": "Test Skin",
  "version": "1.0.0",
  "manifest": "manifest.json",
  "thumbnail": "file:assets/body/idle/stand.gif"
})JSON");
```

Add or update cases:

- `discoverInDirectories(QStringList{userDir, appDir})` returns user-dir descriptor first when both contain the same id.
- invalid root `skin.json` does not shadow a valid child skin.
- missing `manifest.json` skips descriptor.
- unsupported `skinSchemaVersion` skips descriptor.
- `loadFromDirectory()` loads a local filesystem skin and all animation/audio URLs are local files.
- generated clip URL for manifest `clips.<id>` resolves to `file:generated/clips/{id}.gif`.
- missing generated clip for a declared clip fails load with a clear error.
- no test calls `loadFromResource()`.
- no test asserts `descriptor.builtin` or `manifest.builtin`.

- [ ] Update `apps/desktop/src/pet/manifest/SkinDescriptor.h`.

Target shape:

```cpp
struct SkinDescriptor {
    QString id;
    QString name;
    QString version;
    QString author;
    QString license;
    int skinSchemaVersion = 1;
    QString minAppVersion;
    QUrl thumbnailUrl;
    QUrl rootUrl; // filesystem skin root directory
    QString manifestPath; // absolute filesystem path
};
```

- [ ] Update `apps/desktop/src/pet/manifest/SkinManifest.h`.

Remove:

```cpp
bool builtin = false;
```

Keep `skinRootUrl` as the runtime root-of-trust for resource resolution and persona decisions.

- [ ] Update `apps/desktop/src/pet/manifest/SkinManifestLoader.h`.

Target public API:

```cpp
class SkinManifestLoader {
public:
    static SkinManifest loadFromDescriptor(const SkinDescriptor &descriptor);
    static SkinManifest loadFromDirectory(const QString &filesystemPath);
    static SkinManifest fallbackManifest();

    static QList<SkinDescriptor> discoverAll();
    static QList<SkinDescriptor> discoverInDirectories(const QStringList &directories);

    static QString userSkinDirectoryPath();
    static QString appSkinDirectoryPath();

    static QUrl resolveSkinUrl(const QString &rawUrl, const QUrl &rootUrl);
};
```

Remove:

```cpp
static SkinManifest loadFromResource(const QString &resourcePath);
static QString portableSkinDirectoryPath();
static QList<SkinDescriptor> discoverInDirectories(const QStringList &directories, bool includeBuiltins = true);
```

- [ ] Update `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`.

Implementation requirements:

1. `discoverAll()` scans user skin directory before app skin directory:

```cpp
return discoverInDirectories(QStringList{userSkinDirectoryPath(), appSkinDirectoryPath()});
```

2. `appSkinDirectoryPath()` returns:

```cpp
return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("skins"));
```

3. `discoverInDirectories()`:

- accepts only filesystem directories.
- scans root `skin.json`, then one-level child skin directories.
- de-duplicates by `id`; first valid descriptor wins.
- has no hardcoded Miles fallback descriptor.

4. `descriptorFromSkinJson()`:

- reads `skinSchemaVersion`, not `manifestVersion`.
- rejects unsupported schema versions.
- resolves `thumbnail` through strict `file:` resolution.
- sets `manifestPath` to an absolute filesystem path under `rootUrl`.
- rejects missing `manifest.json`.
- does not populate `builtin`.

5. `resolveSkinUrl()`:

```cpp
QUrl SkinManifestLoader::resolveSkinUrl(const QString &rawUrl, const QUrl &rootUrl)
{
    if (!rootUrl.isLocalFile() || rawUrl.isEmpty()) {
        return {};
    }
    if (!rawUrl.startsWith(QStringLiteral("file:"))) {
        return {};
    }

    const QString relative = rawUrl.mid(QStringLiteral("file:").size());
    if (relative.isEmpty() || QDir::isAbsolutePath(relative)) {
        return {};
    }

    const QDir rootDir(rootUrl.toLocalFile());
    const QString candidate = QDir::cleanPath(rootDir.absoluteFilePath(relative));
    const QString rootPath = QDir::cleanPath(rootDir.absolutePath());
    if (candidate != rootPath && !candidate.startsWith(rootPath + QDir::separator())) {
        return {};
    }

    return QUrl::fromLocalFile(candidate);
}
```

6. Keep resource existence checks local-file based. For files that must exist at load time, check `QFileInfo(url.toLocalFile()).isFile()`. For runtime-played optional media, keep the resolved URL local and let the playback path re-check existence before use.

- [ ] Update `apps/desktop/src/pet/PetRuntimeSkin.cpp`.

Remove `builtin` from skin list payload:

```cpp
// Delete:
skin.insert(QStringLiteral("builtin"), descriptor.builtin);
```

- [ ] Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build --output-on-failure -R "skin_manifest_loader_smoke"
```

Expected: smoke test passes and no loader code path mentions qrc built-in skins.

## Task 4: Move Persona Overrides To `skin-overrides`

**Purpose:** 人设读写不再依赖 `builtin`。应用目录皮肤不可直接写，写入用户数据覆盖层；用户目录皮肤可直接写回皮肤目录。

- [ ] Update `apps/desktop/tests/skin_manifest_loader_smoke.cpp` PersonaStore cases first.

Required test shape:

```cpp
SkinManifest appManifest;
appManifest.id = QStringLiteral("app-skin");
appManifest.skinRootUrl = QUrl::fromLocalFile(appSkinRoot.path() + QLatin1Char('/'));

SkinManifest userManifest;
userManifest.id = QStringLiteral("user-skin");
userManifest.skinRootUrl = QUrl::fromLocalFile(userSkinRoot.path() + QLatin1Char('/'));

require(PersonaStore::writeForManifest(userManifest, QStringLiteral("user persona")),
        "user-dir skin persona should be written into skin directory");
require(QFileInfo(userSkinRoot.filePath("persona.md")).isFile(),
        "user-dir persona.md should exist inside skin root");

require(PersonaStore::writeForManifest(appManifest, QStringLiteral("app override")),
        "app-dir skin persona should be written into override directory");
require(QFileInfo(QDir(appDataDir).filePath("skin-overrides/app-skin/persona.md")).isFile(),
        "app-dir override should use skin-overrides/{skinId}/persona.md");
```

The test must set `QCoreApplication::setApplicationName(...)` and standard paths consistently with existing smoke setup so `AppDataLocation` is deterministic.

- [ ] Update `apps/desktop/src/pet/manifest/PersonaStore.cpp`.

Target helpers:

```cpp
QString overridePathForSkin(const QString &skinId)
{
    return QDir(dataDir()).filePath(QStringLiteral("skin-overrides/%1/persona.md").arg(skinId));
}

bool isUnderDirectory(const QString &path, const QString &directory)
{
    const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString cleanDirectory = QDir::cleanPath(QFileInfo(directory).absoluteFilePath());
    return cleanPath == cleanDirectory || cleanPath.startsWith(cleanDirectory + QDir::separator());
}

bool isAppDirectorySkin(const QUrl &rootUrl)
{
    if (!rootUrl.isLocalFile()) {
        return false;
    }
    return isUnderDirectory(rootUrl.toLocalFile(), SkinManifestLoader::appSkinDirectoryPath());
}
```

Behavior:

- `readForDescriptor()` reads app-dir override first, then skin `persona.md`.
- `readForDescriptor()` for user-dir skin reads skin `persona.md` directly.
- `writeForManifest()` writes app-dir skin to override path.
- `writeForManifest()` writes user-dir skin to `persona.md` under skin root.
- qrc root handling is removed.

- [ ] Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build --output-on-failure -R "skin_manifest_loader_smoke"
python3 tests/check_phase_2_3_2_session_persona.py
```

Expected: all pass; no `persona-overrides` or `manifest.builtin` remains.

## Task 5: Package Official Miles As A Filesystem Skin

**Purpose:** 构建产物旁边必须有 `skins/miles-edgeworth/`，桌面 app 与 smoke test 都能通过 `applicationDirPath()/skins` 找到官方皮肤。

- [ ] Update `apps/desktop/CMakeLists.txt`.

Remove generated qrc variables/functions:

```cmake
set(MILES_GENERATED_CLIPS_QRC ...)
function(add_miles_generated_clips_qrc target_name)
...
endfunction()
add_miles_generated_clips_qrc(...)
```

Keep clip generation, but use a stamp file:

```cmake
set(MILES_BUILTIN_SKIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/resources/skins/miles-edgeworth")
set(MILES_GENERATED_CLIPS_STAMP "${CMAKE_CURRENT_BINARY_DIR}/generated/miles-clips.stamp")

add_custom_command(
    OUTPUT "${MILES_GENERATED_CLIPS_STAMP}"
    COMMAND "${Python3_EXECUTABLE}" "${MILES_SPLIT_MANIFEST_CLIPS_SCRIPT}" "${MILES_BUILTIN_SKIN_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${CMAKE_COMMAND}" -E touch "${MILES_GENERATED_CLIPS_STAMP}"
    DEPENDS
        "${MILES_SPLIT_MANIFEST_CLIPS_SCRIPT}"
        "${MILES_BUILTIN_SKIN_ROOT}/manifest.json"
        "${MILES_BUILTIN_SKIN_ROOT}/skin.json"
    WORKING_DIRECTORY "${MILES_REPOSITORY_ROOT}"
    COMMENT "Generating Miles Edgeworth manifest clips"
    VERBATIM
)

add_custom_target(GenerateMilesClips DEPENDS "${MILES_GENERATED_CLIPS_STAMP}")
```

Add a reusable copy function:

```cmake
function(add_miles_skin_copy target_name)
    add_dependencies(${target_name} GenerateMilesClips)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E rm -rf
                "$<TARGET_FILE_DIR:${target_name}>/skins/miles-edgeworth"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/skins"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${MILES_BUILTIN_SKIN_ROOT}"
                "$<TARGET_FILE_DIR:${target_name}>/skins/miles-edgeworth"
        COMMENT "Copying Miles Edgeworth skin beside ${target_name}"
        VERBATIM
    )
endfunction()
```

Call it for every executable that loads official skin resources:

```cmake
add_miles_skin_copy(MilesEdgeworthDesktop)
add_miles_skin_copy(SkinManifestLoaderSmoke)
add_miles_skin_copy(PetRuntimeSmoke)
add_miles_skin_copy(SettingsServiceSmoke)
```

If `ChatControllerSmoke` or another smoke test begins loading real skin resources after implementation, add it to this list in the same task.

Ordering rule:

- For macOS codesign, add `add_miles_skin_copy(MilesEdgeworthDesktop)` before the codesign `POST_BUILD` command so the copied skin is inside the bundle before signing.

- [ ] Update `apps/desktop/resources/pet_assets.qrc`.

Target file should only keep non-skin resources, for example:

```xml
<RCC>
  <qresource prefix="/icon">
    <file>icons/app-icon.png</file>
  </qresource>
</RCC>
```

Do not keep `/pet`, `/audio`, `/skins/miles-edgeworth`, or generated clip entries.

- [ ] Run:

```bash
cmake --build build --target GenerateMilesClips MilesEdgeworthDesktop SkinManifestLoaderSmoke PetRuntimeSmoke SettingsServiceSmoke
test -d build/apps/desktop/skins/miles-edgeworth || test -d build/apps/desktop/MilesEdgeworthDesktop.app/Contents/MacOS/skins/miles-edgeworth
python3 tests/check_phase_1_4_skin_package_contract.py
```

Expected: generated clips exist under the source skin root after generation, and copied skin directory exists next to built executables.

## Task 6: Update Runtime Defaults And Smoke URL Expectations

**Purpose:** Runtime 默认加载路径与测试断言从 qrc URL 切到随包文件 URL。

- [ ] Update `apps/desktop/src/pet/PetRuntime.cpp`.

Current fallback constant uses qrc:

```cpp
constexpr auto kFallbackAnimationUrl = "qrc:/pet/stand-right.gif";
```

Replace with local packaged skin fallback. Keep it small and deterministic:

```cpp
QUrl fallbackAnimationUrl()
{
    const QString path = QDir(SkinManifestLoader::appSkinDirectoryPath())
        .filePath(QStringLiteral("miles-edgeworth/assets/body/idle/stand-right.gif"));
    return QUrl::fromLocalFile(path);
}
```

Use this helper wherever the old `kFallbackAnimationUrl` was used. If the helper needs to live in `SkinManifestLoader.cpp` instead, expose it only if tests need it; avoid adding public API just for convenience.

- [ ] Update `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp` `fallbackManifest()`.

Fallback should be filesystem-based and reference the packaged official Miles path if present. It must not use `qrc:/pet`.

Target behavior:

- If `appSkinDirectoryPath()/miles-edgeworth/manifest.json` exists, load that directory.
- Otherwise create a minimal manifest with `idle_stand` using the same local fallback URL. This rescue path is only for app startup diagnostics; normal tests should use copied official skin.

- [ ] Update `apps/desktop/tests/pet_runtime_smoke.cpp`.

Add a helper near existing test helpers:

```cpp
QString milesSkinPath(const QString &relative)
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("skins/miles-edgeworth/%1").arg(relative));
}

QString milesSkinUrl(const QString &relative)
{
    return QUrl::fromLocalFile(milesSkinPath(relative)).toString();
}
```

Replace exact qrc expectations:

```cpp
runtime.currentSoundUrl().toString() == "qrc:/skins/miles-edgeworth/assets/audio/voice/holdit0.wav"
```

with:

```cpp
runtime.currentSoundUrl().toString() == milesSkinUrl(QStringLiteral("assets/audio/voice/holdit0.wav"))
```

Replace generated clip expectations similarly:

```cpp
runtime.currentAnimationUrl().toString()
    == milesSkinUrl(QStringLiteral("generated/clips/talking.loop.right.gif"))
```

- [ ] Update `apps/desktop/tests/settings_service_smoke.cpp`.

Where test skin JSON is written, replace:

```json
{"id":"bad/skin","name":"非法 id 皮肤","version":"1.0.0","manifestVersion":1}
```

with:

```json
{"skinSchemaVersion":1,"id":"bad/skin","name":"非法 id 皮肤","version":"1.0.0","manifest":"manifest.json"}
```

Replace test manifest resource refs from:

```json
"animation": "skin:assets/body/idle/stand.gif"
```

to schema v4 shape:

```json
"clip": "file:assets/body/idle/stand.gif"
```

If the existing test is still intentionally checking a legacy migration path, remove that legacy case instead of preserving `skin:`.

- [ ] Run:

```bash
cmake --build build --target PetRuntimeSmoke SettingsServiceSmoke
ctest --test-dir build --output-on-failure -R "pet_runtime_smoke|settings_service_smoke"
```

Expected: runtime smoke uses local `file:` URLs for official Miles animation/audio resources.

## Task 7: Remove Remaining qrc Skin References

**Purpose:** 防止后续有人以为 qrc skin 仍是支持路径。

- [ ] Search:

```bash
rg -n "qrc:/pet|qrc:/audio|qrc:/skins/miles-edgeworth|skin:|loadFromResource|portableSkinDirectoryPath|manifestVersion|persona-overrides|clips\\.qrc|\\bbuiltin\\b" apps/desktop tests tools docs/v2/设计方案
```

- [ ] For `apps/desktop` and `tests`, remove or replace every hit except:

- `pet_assets.qrc` itself may remain as an icon qrc file.
- `builtin` can remain only if it is unrelated to skin resources, for example registry "builtins".
- `docs/v2/设计方案` hits are only acceptable if they explicitly describe historical behavior or future migration notes. Long-term design sections should not present qrc skins as current architecture.

- [ ] Update `tests/check_phase_2_4_phased_animation.py` if it still expects `clips.qrc`.

Required replacement:

```python
require("GenerateMilesClips" in desktop_cmake and "split_manifest_clips.py" in desktop_cmake,
        "CMake must generate pre-cut clips")
require("clips.qrc" not in desktop_cmake,
        "pre-cut clips must be packaged as files, not qrc")
```

- [ ] Run all affected static checks:

```bash
python3 tests/check_phase_0_6_animation_runtime.py
python3 tests/check_phase_0_7_phase_runtime.py
python3 tests/check_phase_0_9_startup_idle_runtime.py
python3 tests/check_phase_0_10_locomotion_runtime.py
python3 tests/check_phase_0_11_hit_zone_runtime.py
python3 tests/check_phase_0_12_double_click_sound_runtime.py
python3 tests/check_phase_0_13_drag_shake_runtime.py
python3 tests/check_phase_0_14_prosecutor_badge_runtime.py
python3 tests/check_phase_0_15_menu_audio_movement_settings.py
python3 tests/check_phase_0_16_menu_tea_sleep_wake.py
python3 tests/check_phase_0_25_legacy_idle_once_pool.py
python3 tests/check_phase_0_33_double_click_recipe_smoke.py
python3 tests/check_phase_0_44_system_tray.py
python3 tests/check_phase_0_56_pet_runtime_service_split.py
python3 tests/check_phase_0_72_audio_capability.py
python3 tests/check_phase_2_4_phased_animation.py
```

Expected: every listed static check passes without requiring qrc skin resources.

## Task 8: Final Verification

**Purpose:** 证明 Phase 2.4.3 迁移没有破坏现有动画链、会话人设和设置路径。

- [ ] Regenerate clips:

```bash
python3 tools/split_manifest_clips.py apps/desktop/resources/skins/miles-edgeworth
```

- [ ] Build targeted runtime tests:

```bash
cmake --build build --target GenerateMilesClips MilesEdgeworthDesktop SkinManifestLoaderSmoke PetRuntimeSmoke SettingsServiceSmoke
```

- [ ] Run targeted CTest:

```bash
ctest --test-dir build --output-on-failure -R "skin_manifest_loader_smoke|pet_runtime_smoke|settings_service_smoke|check_phase_1_4_skin_package_contract|check_phase_1_4_skin_url_migration|check_phase_2_3_2_session_persona|check_phase_2_4_2_precut_clips|test_split_manifest_clips|check_phase_2_4_phased_animation"
```

- [ ] Run direct Python checks:

```bash
python3 tests/test_split_manifest_clips.py
python3 tests/check_phase_1_4_skin_package_contract.py
python3 tests/check_phase_1_4_skin_url_migration.py
python3 tests/check_phase_2_3_2_session_persona.py
python3 tests/check_phase_2_4_2_precut_clips.py
python3 tests/check_phase_2_4_phased_animation.py
```

- [ ] Run qrc residue search:

```bash
rg -n "qrc:/pet|qrc:/audio|qrc:/skins/miles-edgeworth|skin:|loadFromResource|portableSkinDirectoryPath|manifestVersion|persona-overrides|clips\\.qrc" apps/desktop tests tools docs/v2/设计方案
```

Expected: no hits in implementation/test code. Any design doc hit must be intentional and clearly marked as historical or future migration context.

- [ ] Inspect git diff:

```bash
git status --short
git diff --stat
git diff -- apps/desktop/resources/pet_assets.qrc apps/desktop/CMakeLists.txt apps/desktop/src/pet/manifest/SkinManifestLoader.cpp
```

Expected:

- `pet_assets.qrc` no longer embeds skin resources.
- CMake generates clips and copies `skins/miles-edgeworth/` beside app/test executables.
- loader has no qrc built-in descriptor or `loadFromResource()`.
- persona overrides use `skin-overrides/{skinId}/persona.md`.

## Suggested Commit Slices

- [ ] `test: 更新皮肤文件系统加载契约`
- [ ] `feat: 移除 clips qrc 生成`
- [ ] `feat: 改为文件系统发现皮肤`
- [ ] `feat: 随包复制 Miles 皮肤`
- [ ] `fix: 改用 skin-overrides 保存随包皮肤人设`
- [ ] `test: 清理 qrc 皮肤旧断言`

## Notes For Future User Skins

After this phase, adding a new local user skin should require only:

```text
<AppDataLocation>/skins/<skin-id>/
├── skin.json
├── manifest.json
├── persona.md
├── assets/
└── generated/clips/
```

For a skin author:

```bash
python3 tools/split_manifest_clips.py /path/to/skin-root
```

Then copy the whole skin root into `<AppDataLocation>/skins/`. No qrc edit, no CMake edit, no C++ edit.
