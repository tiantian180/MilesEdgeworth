## 模型与运行环境说明

本次 review 任务由调度任务 `phase-1-review` 触发，运行模型由调度入口决定。任务文件要求使用 Opus 4.7 extra high；当前会话无法在不可见的元数据外自验证模型版本，故在此醒目标注：**若用户在调度配置中未显式将模型指定为 Opus 4.7 extra high，请按用户期望复跑本任务**。报告本身已按要求基于完整代码与文档生成，不依赖会话历史。

---

# Phase 1 收官 review（2026-05-20）

## 1. 概要结论

Phase 1「旧版手感还原」具备收官条件。Phase 0.65 – 0.73 的五个子阶段均已落地：架构静态门禁、ClickBehavior / GestureTracker / rest / canvas / Audio capability 全部完成；CustomInteraction Host API 与检察官徽章 CI 已经把 Miles 专属玩法挪出框架层；ExpressionMapping schema 已可接收 `agent.expressionRequested` 事件并解析成 `ActionRequest`，为 Phase 2 接 Go sidecar 留好入口。`docs/v2/阶段记录/v1 手感回归与定制化接入.md` 第 2 节列出的"框架内 Miles 硬编码点"在 `PetRuntime` / `InteractionPipeline` / `HitZoneMatcher` / `PetSurfaceWindow` 等通用层中已基本消失，仅余 3 处属于"框架默认值/容错路径"性质的边角硬编码（`SkinManifestLoader::fallbackManifest()`、`HitZoneMatchContext` 默认 240×240 画布、`DesktopShellController` 旧版身体边界常量），均不阻塞 Phase 1 收官，可作为 Phase 0.74+ 跟进项。

**阻塞项数量：0。**「强烈建议尽快修复」级风险：0。「Phase 2 之前应处理」级风险：3。「可与 Phase 2 并行清理」级风险：4。

## 2. Phase 0.65 – 0.73 收官逐项核查表

| 子阶段 | 关键产物 | 状态 | 证据 | 缺口 |
| --- | --- | --- | --- | --- |
| 0.65 架构对齐与门禁 | `tests/check_phase_0_65_architecture_guardrails.cmake`、Phase 0.66 – 0.73 全部对应 CTest 注册 | 已完成 | `CMakeLists.txt:484-553`（0.65 – 0.73 全部 `add_test` 行）；`tests/check_phase_0_65_architecture_guardrails.cmake:6-49`（已无 expected-fail 项） | 0.65 cmake 脚本本身在 Phase 0.66 – 0.70 收口后已无 `_check_absent` 调用，目前是空守卫；若后续再有新的硬编码债务需要"红线"，可继续往里加。 |
| 0.66 ClickBehavior 配置化 | `clickBehaviors.singleClick/doubleClick` 进入 manifest；`HitZoneMatcher` 只返回 zone id | 已完成 | `apps/desktop/resources/skins/miles-edgeworth/manifest.json`（`clickBehaviors.singleClick`、`clickBehaviors.doubleClick`）；`apps/desktop/src/pet/interaction/HitZoneMatcher.cpp:33-65`；`apps/desktop/src/pet/interaction/InteractionPipeline.cpp:151-208`；`tests/check_phase_0_66_click_behavior_pairing.py` 通过 | — |
| 0.67 GestureTracker 拆分 | `GestureTracker` 独立类，事件 `pointer.dragShake/dragReleased` 进入 manifest behaviorRules | 已完成 | `apps/desktop/src/pet/interaction/GestureTracker.{h,cpp}`；`apps/desktop/src/pet/events/PetEventBridge.cpp:54-85`；`apps/desktop/src/pet/interaction/InteractionPipeline.cpp:210-216`；`tests/check_phase_0_67_gesture_tracker_extracted.py` 通过 | `GestureTracker` 内 `> 1000ms` 时间窗与 `>= 5` 反向次数是旧版常量（`GestureTracker.cpp:23,39`），未做 manifest 化；当前与旧版一致，可暂留。 |
| 0.68 rest / startup / action.completed | `capabilities.rest`、`behaviorTriggers.runtime.started/action.completed`、`movementFacingMap`、`blocksPointerInteraction` 进入 manifest | 已完成 | `apps/desktop/src/pet/manifest/SkinManifest.h:160-242`；`SkinManifestLoader.cpp:117,128-143`；`InteractionPipeline.cpp:218-250`；`PetRuntime.cpp:59-73,619-629`；`tests/check_phase_0_68_runtime_decoupling.py` 通过 | — |
| 0.69 canvas / sizes / surface | `canvas.windowSize/imageSize/idleLoopAction`、`sizes`、`defaultVoiceLanguage` 全部 manifest 化；`PetSurfaceWindow` 不写 `idle_stand`；菜单按 `availablePetSizes` 动态生成 | 已完成 | `manifest.json`（`canvas`、`sizes`、`defaultSize`、`audio.defaultVoiceLanguage`）；`PetRuntime.h:93-94`（`petWindowSize/petImageSize` 读 manifest）；`PetSurfaceWindow.cpp:289-291`（`currentActionAcceptsIdleLoopFinished()`）；`PetContextMenu.cpp:35-58`；`tests/check_phase_0_69_canvas_and_size_externalised.py` 通过 | 三个"框架默认值"型 Miles-isms 仍残留，详见第 3 节。 |
| 0.70 CustomInteraction Host API | `CustomInteraction` 抽象、`CustomInteractionHostApi`、Registry 分发、异常隔离、Q_INVOKABLE 不绕过 | 已完成 | `apps/desktop/src/pet/interaction/CustomInteractionRegistry.{h,cpp}`；`ActionRequest.h:18-103`（`SpawnProp`、`PlaySound`）；`PetRuntime.cpp:213-252`；`tests/check_phase_0_70_custom_interaction_host_api.py` 通过 | `registeredInteractions()/interactionStates()` 是进程级静态容器（`CustomInteractionRegistry.cpp:15-25`），多桌宠/多皮肤场景下会共享；工作报告已登记为剩余债务。 |
| 0.71 检察官徽章 CI | `prosecutor_badge` 仅在 `skins/miles-edgeworth/` 中实现，通用层零字符串；CI 通过 `host.emitRecipe(takeThatRecipe)` 复用 `doubleClick.takeThat` 串联语音 | 已完成 | `src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.{h,cpp}`；`src/skins/miles-edgeworth/MilesEdgeworthInteractions.cpp`；`main.cpp:23-24`；`tests/check_phase_0_71_prosecutor_badge_interaction.py` 通过 | CI 注册顺序敏感（`registerBuiltins` 内部 `clearForTest()`，必须先 builtins 再 Miles），详见第 5 节。 |
| 0.72 Audio Capability | `AudioController` 拆出；`audio.voiceLanguages` 可选；菜单"语音"动态生成；PetRuntime 不再持有 `m_audioMuted/m_currentSoundUrl` | 已完成 | `apps/desktop/src/pet/effects/AudioController.{h,cpp}`；`PetRuntime.h:86-88,101,129`；`PetContextMenu.cpp:86-109`；`tests/check_phase_0_72_audio_capability.py` 通过 | — |
| 0.73 ExpressionMapping schema | `manifest.expressions` + `expressionMappings`；`ExpressionMappingResolver`；`PetEvent.agentExpressionRequested`；未知 state 回退到当前 state | 已完成 | `manifest.json`（`expressions/expressionMappings`，schemaVersion=3）；`apps/desktop/src/pet/selection/ExpressionMappingResolver.{h,cpp}`；`PetEvent.h:130-139`；`InteractionPipeline.cpp:264-271`；`PetRuntime.cpp:306-326`；`tests/check_phase_0_73_expression_mapping.py` 通过；smoke 用例 `runtime.submitExpressionRequest("speaking","objection",0.0)` 等 | — |

`apps/desktop/tests/pet_runtime_smoke.cpp` 含 134 个 `require()` 断言，覆盖启动入场、单击 6 个 zone、双击四语音、检察官徽章 CI、确定性随机、未知 state 回退、Audio Capability 静音/语言切换、ExpressionMapping 等关键回归点（详见 `apps/desktop/CMakeLists.txt:117-176` 与 `pet_runtime_smoke.cpp:252-760`）。

## 3. Miles 硬编码残留盘点

`v1 手感回归与定制化接入.md` 第 2 节当前已无显式条目（只剩一行"已收口"占位），下表是本次 review 实际扫描后的残留发现。所有残留都位于"框架默认值 / 容错路径 / 与旧版手感强绑定的几何常量"，**生产路径在加载真实 Miles manifest 时不会触达**，因此不阻塞 Phase 1 收官。

| 清单条目 | 当前位置 | 状态 | 建议 |
| --- | --- | --- | --- |
| `kFallbackActionId = "idle_stand"`、`kFallbackAnimationUrl = "qrc:/pet/stand-right.gif"` 出现在框架 loader 里 | `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp:9-10,597-625`；`apps/desktop/src/pet/PetRuntime.cpp:13,359-364,436` | 残留（框架兜底） | `SkinManifestLoader::fallbackManifest()` 整段都是 Miles 参数（120×100 canvas、`"jp"`/`日语`、`"right"/"left"` facings、`idle_stand`）。建议拆成"空 fallback"（仅返回最小可用空 manifest 的标记）+ Miles 皮肤侧自带 "rescue" manifest；或在该函数顶部明确注释"仅用于 Miles 示例皮肤资源缺失场景"。 |
| `HitZoneMatchContext.canvasWidth/canvasHeight = 240.0` 默认值 | `apps/desktop/src/pet/interaction/HitZoneMatcher.h:19-20` | 残留（隐式 Miles 假设） | 当前 `InteractionPipeline.cpp:162-165` 构造 context 时只填 `facing` / `defaultFacing`，canvas 走 struct 默认值。建议把 `manifest.canvas.windowSize/imageSize` 或专门的 `hitZoneCanvasSize` 字段穿进来，让其他皮肤可以使用不同的 hit zone 画布尺寸；这一项在引入第二个示例皮肤前修。 |
| `DesktopShellController` 中的旧版身体边界常量 `45/50/36/63/10/90 * scale` | `apps/desktop/src/DesktopShellController.cpp:233-234,266-269,291-294` | 残留（强绑定旧版手感） | 这些是旧版 Miles 身体框，与 Phase 0.45 / 0.46 强绑定。建议下一步加 `manifest.surface.bodyBounds`（或 capability）让皮肤声明 `startupOffset` / `bodyClampRect`，把这部分搬出 framework 层。 |
| `PetRuntime` 还在用 `m_petScale = 2.0`、`m_currentFacing = "right"` 作为构造期默认值；`resolveRecipeFacing` 在 `facings.size() != 2` 时再退回 `"right"/"left"` 字面量 | `apps/desktop/src/pet/PetRuntime.h:188,195`；`apps/desktop/src/pet/PetRuntime.cpp:147-153,549-577` | 残留（弱 Miles 偏好） | 构造期默认值会被随后的 `m_manifest.defaultFacing` / `setPetSize(m_manifest.defaultSizeId)` 覆盖；现状不会触达 Miles 字面量。但建议初始化改为依赖 manifest（构造期就 lazy-load 出 manifest 再赋值）或 `QString{}`，把 fallback 路径里的 "right"/"left" 字面量替换为遍历 `m_manifest.facings`。 |

> 框架代码注释里仍会出现"Miles" 词样（例如 `HitZoneMatcher.h:13` "Miles 皮肤的 240x240 逻辑画布"、`CustomInteractionRegistry.cpp:206` "Miles 专属玩法"），这些是阐释性中文注释，不构成代码层硬编码，保留有助于维护理解。

## 4. 测试与 CTest 门禁盘点

测试体量：`tests/` 目录 66 个 Python 检查脚本 + 1 个 cmake 守卫脚本，全部已通过 `CMakeLists.txt` 顶层 `add_test` 注册（共 67 个 add_test，与文件数 1:1 匹配）；另有 `apps/desktop/CMakeLists.txt:117-175` 注册的 C++ `PetRuntimeSmoke`（含 134 条 `require()` 断言）。本机以 `python3 tests/check_phase_0_*.py` 全量回放 0.66 – 0.73：66 个 Python 检查 **66 通过 / 0 失败**；cmake 0.65 守卫脚本目前没有 `_check_absent` 项，等同空守卫。

**已具备的覆盖**：

- 静态结构断言：清单关键字段、源码符号、CMake 配置（Phase 0.66 – 0.73 检查脚本均同时核对 manifest + C++ 头/实现 + CMake 注册）。
- 真实运行时回归：`PetRuntimeSmoke` 经 PetEventBridge / InteractionPipeline 走完整链路，覆盖 8 方向移动、单击 6 zone、双击四语音、Audio Capability 切换、检察官徽章 CI（含确定性随机入口）、`agent.expressionRequested` + 未知 state 回退、`hold` / `once` 模式分支。
- 启动入场屏蔽点击/双击：`pet_runtime_smoke.cpp:282-298` 验证 `briefcase_in` 期间忽略输入。
- CustomInteraction：观察型、skipDefault、Stateful、ScheduledCallback、Throwing、stopPropagation 五种典型 CI 行为全覆盖。

**缺口建议**：

| 缺口 | 建议 |
| --- | --- |
| `tests/check_phase_0_65_architecture_guardrails.cmake` 已无 `_check_absent` 项，但仍作为 CTest 用例 | 可保留作为后续债务的红线挂载点，但建议在脚本头注释"如长期为空则废弃"，避免误以为它在守门。 |
| `pet_runtime_smoke` 中 `require()` 失败时直接 `std::exit(1)`，CI 看不到上下文 | 可在 `require()` 内追加 `std::cerr` 时一并打印当前 `currentActionId/currentState/currentSoundUrl`，方便回归失败时定位。 |
| 没有针对 `SkinManifestLoader::fallbackManifest()` 的单元测试 | 该函数是"皮肤资源不可加载"时的安全网，但没有任何用例触达。建议加一个加载坏 manifest 的 smoke，确认 fallback 路径不崩溃且能播 idle。 |
| `HitZoneMatchContext` 默认 canvas 没有断言 | 在 Phase 0.69 检查中追加 "InteractionPipeline 必须把 manifest.canvas 写入 HitZoneMatchContext"，否则下一个皮肤接入时静默退化到 240×240。 |
| 没有 `DesktopShellController` 的多屏 + clamp 单元测试 | Phase 0.47 检查覆盖了菜单层，但 `clampedPetWindowPosition()` 的真实坐标演算没有 C++ 用例；多屏布局变更或重构时容易回归。 |
| 没有 `voiceLanguages` 为空的多语种 fallback 用例 | 已有 "只声明 default 不声明 list" 的断言（`pet_runtime_smoke.cpp:274-276`），但缺"完全没有 audio 段"的最小皮肤场景，建议补一条。 |

CTest 编译/构建本身需要 macOS + Qt6 + Ninja 环境，沙箱中无法调用；Python 与 cmake 脚本子集已绿色。建议在合并到主分支前在用户机器执行一次 `cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew && cmake --build build && ctest --test-dir build --output-on-failure`，与 Phase 0.65-0.73 工作报告第 7 节列出的清单一致。

## 5. 代码质量与潜在风险

按严重程度排序。"严重"级会影响 Phase 1 收官判定；"中"级建议 Phase 2 之前消化；"低"级可与 Phase 2 并行清理。

### 中：注册顺序耦合（潜在 bug）

`apps/desktop/src/main.cpp:23-24`

```
CustomInteractionRegistry::registerBuiltins(petRuntime.manifest());
registerMilesEdgeworthInteractions(petRuntime.manifest());
```

`CustomInteractionRegistry::registerBuiltins()` 内部第一行调用 `clearForTest()`（`CustomInteractionRegistry.cpp:202-203`），会清空 `registeredInteractions()/interactionStates()`。如果未来在 builtins 之前调用 `registerMilesEdgeworthInteractions`（例如把皮肤层放到 PetRuntime 构造里），Miles handler 会被静默清空，且不报错。

**建议**：把 `clearForTest()` 重命名为 `_clearForTest`、并改为只供测试使用；正式入口 `registerBuiltins` 改为幂等（已存在的 builtin id 不重复添加，不清空）。同时在 `registerInteraction` 里对重复 id 做日志或返回 bool。

### 中：HitZoneMatchContext 隐藏 Miles 假设

`apps/desktop/src/pet/interaction/HitZoneMatcher.h:19-20`、`InteractionPipeline.cpp:162-165`

`canvasWidth/canvasHeight` 默认 240×240，是 Miles 皮肤的特定 hit zone 画布；`InteractionPipeline` 构造 context 时没传 canvas，下一个不同 canvas 的皮肤接入会静默用 Miles 尺寸归一化。

**建议**：把 manifest 里的 hit zone 画布尺寸（建议新增 `manifest.canvas.hitZoneSize` 或复用 `manifest.canvas.windowSize/imageSize`）显式穿到 `HitZoneMatchContext`，去掉 struct 默认值。

### 中：DesktopShellController 中的旧版身体几何常量

`apps/desktop/src/DesktopShellController.cpp:222-300`

`legacyStartupPosition()`、`clampedPetWindowPosition()` 把 Miles 角色身体框 `(36,10,63,90)` 与 `45/50/90 * scale` 直接写进框架。注释明确说明是旧版手感，但常量不可换肤。

**建议**：在 manifest 加 `surface.bodyBounds` / `surface.startupOffset` 字段，DesktopShellController 在 `setPetWindow()` 阶段从 manifest 读取一次；当前 Miles 数值作为该字段的默认值即可。

### 低：CustomInteraction Registry 是进程级单例

`apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp:15-25`

`registeredInteractions()`/`interactionStates()` 是函数局部静态容器。多 PetRuntime / 多 SkinSession 共用一份。工作报告 §6 已登记，Phase 2 之前不需要修。

**建议**：作为 Phase 0.74+ debt，迁到 `PetRuntime` 或 `SkinSession` 持有；可与 PetRuntime 拆分 (`PlaybackController` / `RecipeRunner`) 同期处理。

### 低：异常静默吞掉

`apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp:246-251`

```
} catch (const std::exception &) {
    continue;
} catch (...) {
    continue;
}
```

设计意图是"皮肤脚本崩溃不破坏主流程"，没问题，但 swallow 后无任何日志，调试 JS adapter 时很难复盘。

**建议**：项目目前不引入 logging 模块；可在 catch 块中先 `qWarning` 一行 handler id + exception what()，等价小补丁，影响面可控。注意 `apps/desktop/src` 整层目前 0 个 `qWarning/qDebug`（grep 显示），可以借这个机会建立"只有 framework 层异常分支允许打 qWarning"的约定。

### 低：PetRuntime 构造期默认值仍含 Miles 偏好

`apps/desktop/src/pet/PetRuntime.h:188,195`

`m_currentFacing = "right"`、`m_petScale = 2.0`。运行期会被 manifest 覆盖，不会暴露，但读起来有"框架默认就是 Miles"的味道。

**建议**：默认值改为空串和 `0.0`，构造体在 manifest 解析后再赋值。

### 低：手势识别常量未 manifest 化

`apps/desktop/src/pet/interaction/GestureTracker.cpp:23,39`

`>1000ms` 时间窗、`>=5` 反向次数是旧版常量。改成 manifest 可配置可以让"灵敏度"调节成皮肤能力，但当前一致复刻旧版手感即可。

### 低：playActionInternal 中途换 manifest 的潜在悬挂

`apps/desktop/src/pet/PetRuntime.cpp:358-365`

当 manifest 中没有目标 action 时，代码 in-place 把 `m_manifest` 整个替换成 `SkinManifestLoader::fallbackManifest()`。此时如果有任何缓存的 `const SkinManifest &` 引用（例如未来异步 task / Custom Interaction Host API 里持有），会发生悬挂。

**当前**：`CustomInteractionHostApi` 的 `m_manifest` 是 const-ref，按值传入引用，调用结束后释放，理论安全。`InteractionPipeline::handleEvent` 同样调用即用，无跨事件持有。所以目前没问题。

**建议**：把 `m_manifest` 重新赋值这条 fallback 路径加注释明确"必须在 handle 链路外执行"，或干脆改为"启动期一次性 fallback，运行期不再热替换 manifest"。

### 备注（不算风险）

- `PetRuntime.cpp` 706 行、`SkinManifestLoader.cpp` 636 行，已经是文件体量上限；工作报告 §6 已经登记进 Phase 0.74+ 拆分。
- `apps/desktop/src` 全树 0 个 `TODO/FIXME/XXX/HACK` 残留，0 个 `qDebug/qWarning`，注释密度高且为中文：代码质量整体上等水准。

## 6. 收官行动项

按优先级排序，可直接转工单。

1. **[阻塞前必做 · P1]** 在用户本地执行一次完整链路验证：`cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew && cmake --build build && ctest --test-dir build --output-on-failure` 与 `apps/desktop/qml/PetWindow.qml` 的 qmllint（沙箱无法跑），确认 macOS Qt6 真实环境下 67 个 Python 检查 + `PetRuntimeSmoke` 全部通过；缺一个就不进入 Phase 2。
2. **[P1]** `CustomInteractionRegistry::registerBuiltins` 改为幂等并去掉 `clearForTest()` 副作用；新增 "重复 id 注册" 的 smoke 用例。降低 main.cpp 注册顺序耦合。
3. **[P2]** 在 `InteractionPipeline::handleEvent` 构造 `HitZoneMatchContext` 时显式注入 manifest canvas 尺寸；去掉 `HitZoneMatchContext` 的 240×240 默认值；并在 Phase 0.69 检查脚本里追加断言。
4. **[P2]** 把 `SkinManifestLoader::fallbackManifest()` 拆成 "framework 空骨架 fallback" 与 "Miles 示例皮肤资源缺失 rescue manifest"，后者迁到 `skins/miles-edgeworth/`；同时补一条 "manifest 加载失败" 的 smoke 用例。
5. **[P2]** 把 `DesktopShellController` 的 `legacyStartupPosition` / `clampedPetWindowPosition` 中的 Miles 身体框常量 manifest 化（新增 `surface.bodyBounds` / `surface.startupOffset`），框架代码只读字段；Phase 0.45 / 0.46 检查同步迁移。
6. **[P3]** `CustomInteractionRegistry` 的 catch 块加 `qWarning` 一行 handler id + what()；约定框架层异常分支允许打 qWarning。
7. **[P3]** `PetRuntime` 构造期默认 `m_currentFacing` / `m_petScale` 改为空与 0.0；运行期由 manifest 单一来源赋值。
8. **[P3]** 把 `tests/check_phase_0_65_architecture_guardrails.cmake` 加注释，明确"空守卫 = 当前无活跃硬编码债务，未来如有新条目再添加"。
9. **[P4 · Phase 0.74+]** 启动 `PetRuntime` 拆 `PlaybackController` / `RecipeRunner`（工作报告 §6 已记录），与 `CustomInteractionRegistry` 单例下沉到 `SkinSession` 同期推进，不要和 Phase 2 聊天主链路混在一个提交里。
10. **[P4]** `GestureTracker` 的时间窗 / 反向次数迁到 manifest（可选 capability），方便后续皮肤调"灵敏度"。

完成 1 – 2 项即可宣告 Phase 1 收官；3 – 5 项建议在 Phase 2 启动会议之前消化；其余可与 Phase 2 并行。

## 7. 本轮补修结果

根据本 review，已完成以下低风险收口项：

- `CustomInteractionRegistry::registerBuiltins()` 改为幂等注册，不再清空生产 Registry；重复 handler id 会被跳过。
- `PetRuntimeSmoke` 增加注册顺序和重复 id 回归断言，防止内置 CI 与皮肤 CI 后续再次出现顺序耦合。
- `CustomInteractionRegistry` 的异常隔离分支增加 `qWarning`，记录 handler id 与异常信息。
- 新增 `manifest.canvas.hitZoneSize`，`InteractionPipeline` 显式把该值传入 `HitZoneMatchContext`；`HitZoneMatchContext` 不再默认 240×240。
- `PetRuntime` 构造期默认 `m_currentFacing` / `m_petScale` 改为空与 `0.0`，由 manifest 初始化后的值作为来源。
- `tests/check_phase_0_65_architecture_guardrails.cmake` 标注当前没有活跃 `_check_absent` 调用。

仍保留为后续阶段处理：

- `SkinManifestLoader::fallbackManifest()` 拆成框架空 fallback 与 Miles rescue manifest。
- `DesktopShellController` 身体边界与启动偏移迁入 `manifest.surface`。
- `CustomInteractionRegistry` 从进程级单例下沉到 `SkinSession` / Runtime 持有。

---

## review 方法说明

本次检查覆盖：

- **文档导览**：`docs/v2/文档索引.md`、`docs/v2/设计方案/总体架构设计.md`（"Phase 1 末期收官顺序"）、`docs/v2/阶段记录/v1 手感回归与定制化接入.md`、`docs/v2/阶段记录/Phase 0.65-0.73 工作报告.md`。
- **测试脚本**：`tests/` 下 67 个文件全数列举；详细阅读 `check_phase_0_65_architecture_guardrails.cmake`、`check_phase_0_66_click_behavior_pairing.py`、`check_phase_0_67_gesture_tracker_extracted.py`、`check_phase_0_68_runtime_decoupling.py`、`check_phase_0_69_canvas_and_size_externalised.py`、`check_phase_0_70_custom_interaction_host_api.py`、`check_phase_0_71_prosecutor_badge_interaction.py`、`check_phase_0_72_audio_capability.py`、`check_phase_0_73_expression_mapping.py`；并在沙箱内对 66 个 Python 检查全量回放，66 通过 0 失败。
- **构建脚本**：`CMakeLists.txt`、`apps/desktop/CMakeLists.txt`。
- **源码 / manifest**：`apps/desktop/src/main.cpp`、`apps/desktop/src/DesktopShellController.{h,cpp}`、`apps/desktop/src/pet/PetRuntime.{h,cpp}`、`apps/desktop/src/pet/events/{PetEvent.h,PetEventBridge.{h,cpp}}`、`apps/desktop/src/pet/interaction/{InteractionPipeline,HitZoneMatcher,GestureTracker,CustomInteractionRegistry}.{h,cpp}`、`apps/desktop/src/pet/manifest/{SkinManifest.h,SkinManifestLoader.cpp}`、`apps/desktop/src/pet/effects/{AudioController,PropController}.{h,cpp}`、`apps/desktop/src/pet/selection/ExpressionMappingResolver.{h,cpp}`、`apps/desktop/src/pet/surface/{PetSurfaceWindow,PetContextMenu}.cpp`、`apps/desktop/src/skins/miles-edgeworth/**`、`apps/desktop/tests/pet_runtime_smoke.cpp`、`apps/desktop/resources/skins/miles-edgeworth/manifest.json`。
- **搜索关键词**：`sleep`、`briefcase_in`、`drag_crouch`、`walk.finished` / `run.finished`、`idle_stand`、`voiceLanguage`、`prosecutor`、`takeThat` / `takethat`、`miles.feedTea`、`"jp"` / `"en"` / `"zh"`、`120.0` / `100.0`、`Miles` / `miles` / `edgeworth`、`TODO/FIXME/XXX/HACK`、`qWarning/qDebug/qCritical/qFatal`，限定在 `apps/desktop/src` 及子目录中按文件分组扫描。
- **未覆盖**：未编译执行（沙箱无 cmake + Qt6 工具链），未跑 `qmllint`，未实际触发 CTest 二进制；建议复核时补做。
