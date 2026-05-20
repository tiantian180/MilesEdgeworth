# Phase 1 收尾与体验问题修复计划

日期：2026-05-20

## 1. 当前状态

Phase 0.73 已完成，v2 框架通用层已清晰：

- 框架层不再包含 Miles 专属字符串（`idle_stand`、`drag_crouch`、`briefcase_in`、`voiceLanguage` 等已全部迁入 manifest 或 capability）。
- 单击分区、双击行为、行为触发、rest / startup / action.completed、表达映射均由 manifest 声明驱动。
- Custom Interaction Host API 落地，检察官徽章通过 CI 回归；语音语言通过 AudioCapability 回归；ExpressionMapping 接口已预留。

但对比 v1（`macos-build-support` 分支）的实际手感，以下问题需要在进入 Phase 2 AI 接入之前修复。

---

## 2. 待修复问题

### 2.1 单击分区动画错误（优先级：高）

**表现**：点击腰部触发的是胸部动画，其他区域也存在错位或无响应。

**根因**：manifest 中 `hitZones` 的 zone 边界系统性下移约 30–40px，且部分区域存在覆盖空洞（无 zone 命中时 pipeline 不生成请求，点击无反应）。

具体错位分析（240×240 窗口坐标系）：

| Zone | 应有 y 范围 | 实际 y 范围 | 问题 |
| --- | --- | --- | --- |
| chest | 66–96 | 72–**126** | 下边界过低，吞掉了腰部 |
| belly_bow | 96–112 | **126**–148 | 整体下移 ~30px |
| belly_pointing | 112–126 | **148**–170 | 整体下移 ~36px |
| legs | ≥ 126 | ≥ **166** | 整体下移 ~40px |
| head | y<66, 全宽 | x=78–162 | 左右边界过窄，头部两侧无响应 |

**修复方案**：

短期（Step 1）：直接修正 `manifest.json` 中 `hitZones` 各 zone 的边界值，使其与 v1 逻辑一致；同时补充一个全身 `fallback` zone 放在 `singleClick` 列表末尾，消除空洞。

长期（Step 2）：重设计 HitZone 坐标系和 schema，见 [`设计方案/HitZone 交互区域系统设计.md`](../设计方案/HitZone%20交互区域系统设计.md)。

**Step 1 修正值**（right facing，240×240 窗口空间）：

| Zone | x 范围 | y 范围 | 形状 |
| --- | --- | --- | --- |
| face | polygon | polygon | 见下 |
| head | 20–220 | 20–66 | rect（全宽，face polygon 优先级更高） |
| upper_arm | 20–106 | 66–100 | rect |
| forearm | 20–106 | 100–134 | rect |
| chest | 106–220 | 66–96 | rect |
| belly_bow | 106–220 | 96–112 | rect |
| belly_pointing | 106–220 | 112–126 | rect |
| legs_back | 20–120 | 126–224 | rect |
| legs_look_down | 120–220 | 126–224 | rect |
| **fallback** | 0–240 | 0–240 | rect（新增，置于列表末尾） |

face polygon（right）校正：`[[143, 20], [220, 20], [220, 66], [105, 66]]`

left facing 按 x 轴镜像：left_x = 240 − right_x（对称轴 x=120）。

---

### 2.2 拖拽晃动效果未生效（优先级：中）

**表现**：拖拽并快速左右晃动桌宠，未触发蹲下动画。

**定位**：manifest 中 `behaviorRules` 的字段格式解析正确（loader 已验证），`pointer.dragShake` 事件的 Action 为 `drag_crouch`。问题在于 `GestureTracker` 没有发出该事件，或发出条件过严。

**排查步骤**：
1. 在 `PetSurfaceWindow::mouseMoveEvent` 确认 `handleDragMoved` 被调用。
2. 在 `GestureTracker::updateDrag` 加日志，确认晃动计数是否累积。
3. 检查 `GestureTracker` 的抖动阈值（`kShakeThreshold`）和时间窗口（`kShakeWindowMs`）是否适合鼠标操作速度（v1 的判定是 1 秒内 5 次方向反转）。

**预期修复**：调整 `GestureTracker` 的参数，或补充 manifest `gestureTracker` 字段支持皮肤自定义阈值。

**本轮约束**：先不新增 `gestureTracker` schema。若问题来自事件采样或初始方向逻辑，优先在 `GestureTracker` 内修正识别算法；只有不同皮肤确实需要不同参数时，再把阈值迁入 manifest。

---

### 2.3 检察官徽章尺寸过大（优先级：中）

**表现**：丢出的徽章显示尺寸明显大于 v1。

**定位**：manifest 中 `props.prosecutor_badge` 的 `width` / `height` 为 94，`visualWidth` / `visualHeight` 为 70。当前 `PropController` 把 Prop 尺寸字段当作中号 scale=2 的基准值，再按 `petScale / 2` 缩放，因此 scale=2 时会显示 70px，明显大于 v1。

**修复方案**：
1. 对比 v1 徽章图片的原始像素尺寸和实际显示大小（v1 未做缩放，直接按图片大小显示）。
2. 调整 manifest 的 `visualWidth` / `visualHeight` 值，使 scale=2 时的显示尺寸和 v1 一致。
3. 同时检查 `startOffsets`——徽章出发位置也要和 v1 角色手部位置对齐（当前定义在 240 坐标系下，可能也受 hitZone 坐标系 bug 影响需一并校正）。

---

### 2.4 缩放控件：从离散菜单改为连续拖拽（优先级：低）

**表现**：右键菜单里的"迷你 / 小 / 中 / 大"选项体验粗糙，不如连续拖拽直观。

**归属**：属于"用户偏好"类（见《总体架构设计》§9.x），落在桌宠应用的右键菜单 / 设置面板，不进入开发者工具。manifest 的 `sizes` 仍可作为"快捷档位声明"被皮肤包提供（点击快捷档位等价于把连续拖拽设到那个值）。

**影响范围**：
- `PetContextMenu`：移除 4 个尺寸 action，改为打开一个简单的"缩放"子面板（含 slider + manifest 快捷档位按钮）
- `PetRuntime`：`setPetSize(id)` 改为 `setPetScale(double)`，并把当前 scale 持久化到 `QSettings`
- manifest `sizes` 数组：仍解析，作为快捷档位显示在 slider 旁边的小按钮

**决定推迟**：此项不影响框架架构和 Phase 2 AI 接入，可以作为独立 UX 任务，在 Phase 2 期间插空完成，或作为 Phase 2 收尾任务。

### 2.5 Pet Skin Studio（统一开发者工具，优先级：中）

**归属**：属于"皮肤包内容编辑"类，是独立 Qt 应用，不进入桌宠应用菜单。位置：`apps/pet-skin-studio/`。

**定位**：Pet Skin Studio 是**所有皮肤创作工作的统一容器**，不是单一功能工具。首发只做 HitZone Panel（因为我们当下就需要它），后续按需扩展：

| Panel | 功能 | 何时做 |
| --- | --- | --- |
| HitZone | 可视化划分点击区域 | **首发**（Phase 1 收尾） |
| Action / Recipe | 编辑动作、phase、recipe 时间线 | 下一个需要时 |
| ActionPool | 权重池管理 | 之后 |
| BehaviorRule | 行为规则可视化 | 之后 |
| Asset 浏览 | 浏览皮肤包内素材，预览动画 | 任何时候 |
| Manifest 校验 | 校验 schema / 缺失资源 / 死引用 | 任何时候 |

每加一个 panel 不需要再起一个新工具，避免散乱。

**与桌宠应用的协作**：
- Studio 只读 / 写文件系统皮肤目录的 manifest.json
- 桌宠应用菜单加一个"重载当前皮肤"入口，Studio 保存后到桌宠应用点一下即可看到效果
- 两者共享 `SkinManifest` 数据模型和 `SkinManifestLoader`

详见《HitZone 交互区域系统设计》第 7 节（HitZone Panel 设计）和后续《Pet Skin Studio 工具设计》（待补，承载 Studio 整体框架）。

**决定**：手动改 manifest JSON 也能修 HitZone，所以 Studio 不阻塞 Phase 2 AI 主线工作。但 Studio 的存在依赖于 §2.6 的"文件系统皮肤包"完成，所以这两件事一起规划。

### 2.6 文件系统皮肤包（去编译化分发，优先级：高，结构性）

**问题**：Step 4 前，皮肤只能通过 qrc 编进二进制，意味着每个想换皮肤的用户都得安装 C++/Qt 编译环境。这不符合"皮肤是数据"的初衷，也阻塞 Pet Skin Studio 落地（Studio 无法编辑编进二进制的资源）。

**目标**：让最终用户**只调整素材和配置就能换皮肤**，无需任何编译工具。

**设计**：详见《皮肤包分发与加载机制设计》。核心要点：

- 应用扫描三个路径：用户目录、便携同级目录、内置 qrc
- 新 URL scheme `skin:` 让 manifest 在两种位置都可移植
- `SkinManifestLoader` 加 `loadFromDirectory` / `discoverAll` 接口
- 桌宠应用菜单加"皮肤"子菜单 + "打开皮肤目录" + "重载当前皮肤"
- 内置 Miles 兼容期内保留 qrc，新皮肤都走文件系统

**优先级提升的理由**：这是 Phase 1 收尾的**结构性问题**，不是 polish。延后做会让以后的所有皮肤工作都被"先编译再测试"的循环拖慢；做完后 Pet Skin Studio 才有意义。

**与 Phase 2 的关系**：可与 Phase 2 AI 接入并行；不阻塞 AI 主线，但 AI 应该早点能在不同皮肤上 demo。

---

## 3. 执行顺序

```
Step 1  修正 manifest hitZones 数值（不改代码，直接验证手感）
        ├─ 必须先点击实测：v1 widget 200×200 是推算值，落地数值可能要微调
        └─ 在 singleClick 列表末尾加 fallback zone，消除空洞
        ↓ 验证单击分区手感是否接近 v1
Step 2  排查 GestureTracker 晃动参数
        ├─ 在 mouseMoveEvent / updateDrag 加日志确认事件链路
        └─ 调整 kShakeThreshold / kShakeWindowMs
        ↓ 验证拖拽晃动触发蹲下动画
Step 3  调整徽章 visualWidth / visualHeight + startOffsets
        ↓ 验证徽章尺寸与 v1 一致
─────────────── 以上为 Phase 1 必修 bug ───────────────
Step 4  文件系统皮肤包（§2.6，结构性，已完成）
        ├─ SkinManifestLoader 加 loadFromDirectory + skin: scheme
        ├─ 皮肤发现：扫描 user / portable / qrc 三路径
        ├─ Miles manifest 改用 skin: URL（兼容期保留 qrc:）
        └─ 应用菜单加"皮肤"子菜单 + "重载当前皮肤"
        ↓ Studio 才能编辑磁盘上的皮肤
Step 5  进入 Phase 2：AI 聊天接入
        （Phase 0.73 ExpressionMapping 已就绪，框架入口已打开）

── 以下与 Phase 2 并行，不阻塞 AI 主线 ──
Step 6  HitZone 坐标系和 schema 重设计（见设计文档第 8 节迁移路径）
        ├─ canvas schema 加 windowWidth/Height/imageWidth/Height
        ├─ HitZoneMatcher 坐标映射改用 imageWidth/Height
        ├─ manifest zones 迁移到 image-space 坐标（0–100 空间）
        └─ 旧 schema 字段过渡期内并存，loader 双解析
Step 7  连续缩放控件（用户偏好类，桌宠应用菜单）
Step 8  Pet Skin Studio: HitZone Panel 首发
        ├─ Studio 应用框架（main window、皮肤选择、panel 切换）
        ├─ HitZone Panel（节点式多边形编辑、层面板、实时预览）
        └─ 依赖 Step 4 完成（Studio 编辑文件系统皮肤）
```

**说明**：
- Step 1–3 是 Phase 2 的硬前置（影响 demo 体验）
- Step 4 是结构性改造，已经完成；Phase 2 的 AI demo 可以直接使用文件系统皮肤做快速迭代
- Step 6–8 可以和 Phase 2 AI 接入并行推进；Step 8 依赖 Step 4 + Step 6 完成

---

## 4. Phase 2 接入条件检查

在上述 bug 修复完成后，进入 Phase 2 的前提条件：

| 条件 | 状态 |
| --- | --- |
| 框架通用层无皮肤专属字符串 | ✅ Phase 0.65–0.73 已完成 |
| ExpressionMapping schema 就绪 | ✅ Phase 0.73 已完成 |
| `agent.expressionRequested` 事件入口 | ✅ Phase 0.73 已完成 |
| 单击分区动画正确 | ⬜ Step 1 修复 |
| 拖拽晃动生效 | ⬜ Step 2 修复 |
| 检察官徽章尺寸正确 | ⬜ Step 3 修复 |
| 文件系统皮肤包（去编译化） | ✅ Step 4 已完成 |
| HitZone 坐标系重设计 | ⬜ Step 6，可与 Phase 2 并行 |
| 连续缩放控件 | ⬜ Step 7，可与 Phase 2 并行 |
| Pet Skin Studio: HitZone Panel | ⬜ Step 8，依赖 Step 4 + Step 6 |

Phase 2 主要工作：
1. Go sidecar Agent Core（模型 API、流式回复、历史记录）
2. `ChatWindow.qml` 聊天窗口 UI
3. `PetEventBridge` → `agent.expressionRequested` 事件从 Go sidecar 接入
4. ExpressionMapping 管线端到端测试

---

## 5. 实施记录

### 5.1 v1 对照数据

对照分支：`macos-build-support`。为了避免污染当前分支，使用独立 worktree `/tmp/MilesEdgeworth-v1-probe` 和构建目录 `/tmp/MilesEdgeworth-v1-build`。

确认结果：

- v1 默认尺寸为 `MEDIAN = 2`。
- v1 主体 QLabel 尺寸为 `100 * scale`，默认中号即 `200×200`。
- v2 当前中号窗口为 `240×240`，主体图像为 `200×200`，四周各有 20px padding。
- v1 单击区域来自 `MilesEdgeworth.cpp::singleClickEvent()`，核心阈值是 100 空间下的 `y=23/40/57/38/46/53` 和 `x=43/50/57`。
- v1 检察官徽章 QLabel 尺寸为 `12 * scale`，默认中号实际显示 `24×24`。
- v1 徽章飞出起点为右向 `(86 * scale, 16 * scale)`、左向 `(1 * scale, 16 * scale)`。v2 当前 `startOffsets` 使用中号基准值，因此右向 `(172, 32)`、左向 `(2, 32)` 与 v1 对齐，无需调整。

### 5.2 HitZone 最终落地值

本轮仍使用 240×240 窗口坐标系。落地值如下：

| Zone | right | left |
| --- | --- | --- |
| face | polygon `[[143,20],[220,20],[220,66],[105,66]]` | polygon `[[20,20],[97,20],[135,66],[20,66]]` |
| head | `x=20, y=20, w=200, h=46` | 同 right |
| upper_arm | `x=20, y=66, w=86, h=34` | `x=134, y=66, w=86, h=34` |
| forearm | `x=20, y=100, w=86, h=34` | `x=134, y=100, w=86, h=34` |
| chest | `x=106, y=66, w=114, h=30` | `x=20, y=66, w=114, h=30` |
| belly_bow | `x=106, y=96, w=114, h=16` | `x=20, y=96, w=114, h=16` |
| belly_pointing | `x=106, y=112, w=114, h=14` | `x=20, y=112, w=114, h=14` |
| legs_back | `x=20, y=126, w=100, h=98` | `x=120, y=126, w=100, h=98` |
| legs_look_down | `x=120, y=126, w=100, h=98` | `x=20, y=126, w=100, h=98` |
| fallback | `x=0, y=0, w=240, h=240` | 同 right |

`fallback` 放在 `clickBehaviors.singleClick` 最后。`click.fallback` 使用 `type: returnToIdle`，同时修正了 `returnToIdle()` 在已经待机时仍重启动画的问题，避免 padding 区点击造成视觉 jank。

### 5.3 GestureTracker 诊断结论

失败复现序列：`100 → 110 → 104 → 112 → 103 → 113 → 102`。这组输入代表用户快速左右晃动，但每次不一定跨过最初按下点。

旧识别方式以“上一次反转点/初始点”为基准，只有移动跨过旧基准点才计数；上述真实手工操作会被漏判。修正后改为按连续采样的横向移动方向计数：

- 单次采样移动小于 `4px` 时忽略，过滤像素级抖动。
- 时间窗口保持 `1000ms`。
- 换向阈值改为 `4` 次真实方向切换。原因是修正后的算法不再把第一次移动当成一次换向，旧 smoke 中的五个移动点实际只有四次真实换向。

本轮未新增 manifest `gestureTracker` schema。

### 5.4 检察官徽章尺寸

v1 中号实际显示为 `12 * 2 = 24px`。v2 Prop 尺寸字段当前按中号基准缩放，因此：

- `visualWidth = 24`
- `visualHeight = 24`
- `startOffsets.right = { x: 172, y: 32 }` 保持不变
- `startOffsets.left = { x: 2, y: 32 }` 保持不变

对应缩放结果：

- mini scale=1：显示 12px
- medium scale=2：显示 24px
- big scale=3：显示 36px

### 5.5 验证记录

已执行：

```bash
python3 tests/check_phase_0_32_single_click_zone_smoke.py
python3 tests/check_phase_0_66_click_behavior_pairing.py
python3 tests/check_phase_0_13_drag_shake_runtime.py
python3 tests/check_phase_0_29_drag_shake_smoke.py
python3 tests/check_phase_0_67_gesture_tracker_extracted.py
python3 tests/check_phase_0_30_prosecutor_badge_smoke.py
python3 tests/check_phase_0_43_scaled_prosecutor_badge.py
ctest --test-dir build --output-on-failure -R 'check_phase_0_(13|29|30|32|43|66|67)|pet_runtime_smoke'
```

完整验证在本轮收尾时再统一执行并记录结果。

## Step 4 实施记录：文件系统皮肤包

- 新增 `skin.json` 元信息层。
- 新增 `SkinDescriptor`、`loadFromDirectory()`、`discoverAll()` 和 `skin:` URL 解析。
- 内置 Miles 通过 `qrc:/skins/miles-edgeworth/` 暴露，同时保留历史 `qrc:/pet` / `qrc:/audio` alias。
- 右键菜单新增 `皮肤` 子菜单，支持切换皮肤、重载当前皮肤、打开用户皮肤目录。
- Miles manifest 已迁移到 `skin:assets/...`，文件系统皮肤和内置皮肤共用同一套 manifest URL 写法。

验证命令：

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
```

补充文件系统皮肤冒烟：

- 复制内置 Miles 到应用同级 `skins/miles-edgeworth`。
- 修改 `skin.json` 名称和一处 manifest 文案，确认加载来源可以变成文件系统皮肤。
- 启动 `build/apps/desktop/MilesEdgeworthDesktop.app`，确认桌宠进程正常运行。
- 删除临时便携皮肤目录，避免污染本机后续测试。
