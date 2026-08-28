# 帧编排与导出

将选定的透明 PNG 编排成连续 GIF。工具只处理已有图片，不生成姿势或去背景。

## 准备动作目录

复制 [帧计划模板](../assets/action-plan.json) 为本段 plan.json，把原图放入旁边的 images 目录。按真实图片填写裁切、锚点和时长；模板里的数值只是示例。

源图需为单帧 PNG，有真实透明区域，角色和独立道具完整。每个裁切框至少留一圈透明像素，避免切断轮廓或包含邻帧。缺失的轮廓先用图像编辑工具修好，再导出。

## 帧计划字段

| 字段 | 设置 |
| --- | --- |
| actionId | 小写字母、数字、连字符；最多 64 字符，决定 GIF 文件名 |
| canvas | 统一画布，宽高各 16–1024px |
| targetAnchor | 画布内的目标锚点 |
| scale | 全段统一缩放，范围大于 0 且不超过 8 |
| padding | 透明边距，整数，至少 1 且小于短边的一半 |
| alphaThreshold | GIF 二值透明阈值，1–255，结合浅深底预览选择 |
| loop | true 无限循环；false 不写循环扩展，软件仍按动作语义控制播放 |
| mirrorX / mirrorReason | 可选镜像及原因；先确认非对称标记、服装和道具适合镜像 |
| baselineY | 可选接地高度，填写时至少有一帧带 contactPoint |
| frames | 按时间排列的 2–96 帧，总画布像素不超过 3200 万 |

每帧字段：

- source：相对 plan.json 的正斜杠路径，文件位于该目录内。
- crop：[x,y,width,height]，原图绝对整数坐标。
- anchor：[x,y]，原图绝对坐标，位于 crop 内。
- durationMs：20–10000ms，10ms 的整数倍；延长停顿用时长，不重复相邻帧。
- contactPoint：可选原图整数坐标，用于接地帧。该点须是达到透明阈值的可见像素，下方紧邻透明像素；映射后距 baselineY 不超过 2px，缩放后周围 1px 仍有可见像素。离地帧省略。
- requiredRegions：可选独立道具区域，例如 `[{"label":"star","box":[x,y,w,h]}]`。区域须完整落在 crop 内，导出前后都有可见像素。紧贴目标物体标注，不用大框连同身体一起圈入。

飞行、游动或没有接地检查时同时省略 baselineY 和 contactPoint。栖架、载具的接触若不符合可见下边缘条件，用普通锚点定位，再查看实际接触效果。requiredRegions 检查道具是否仍在，并不判断形状是否正确。

## 锚点与运动

编译器把每帧 anchor 放到同一个 targetAnchor。逐帧追踪移动的身体中心会抵消平移：

- 原地表演可以固定身体或支撑点。
- 保留飞行、跃起、游动的画内位移，用固定画布参考，不逐帧居中。
- 同尺寸独立帧可使用一致 crop 和画布 anchor。
- 同张分镜的不同格使用相同格内参考，再换算成原图坐标：两个 64×64 格起点为 (0,0)、(64,0)，格内 (32,32) 对应 anchor [32,32]、[96,32]。

同一段保持统一 scale。不要按每帧身体包围盒拉满画布；收拢和展开应保留真实大小差异。裁切不同也要保留对应参考，必要时扩大裁切。

## 导出

```text
python <skill-dir>/scripts/compile_action.py <action-dir>/plan.json --out <action-dir>/runs/r001
```

输出目录须为新版本目录。工具保留源图，生成：

```text
r001/
  images/000.png ...       定位后的 RGBA 帧
  gifs/<action-id>.gif     连续 GIF
  qa/light.png dark.png    GIF 解码后的浅深底预览
  plan.json               本次参数
  report.json             文件指纹、帧数、时长和导出结果
```

返回码 0 表示导出完成，1 表示需要调整。若目录内出现 FAILED.txt，使用新 revision 重新导出，不交付该目录。

Pillow 为全段生成统一调色板，保留透明索引，并核对 GIF 解码后的画面与时长。PNG 保留半透明，GIF 是二值透明；以最终 GIF 的实际大小和浅深底效果选择阈值。

## 检查版本与播放

```text
python <skill-dir>/scripts/verify_action.py <action-dir>/plan.json <action-dir>/runs/r001
```

此命令确认当前计划、原图、每帧 PNG、GIF 和预览与导出时相同。文件变更后重新导出；原图和计划仍使用动作目录里的版本，而不是 runs 内的参数副本。

完整观看浅底、深底、实际大小、原速、慢速、循环/退出、道具轮廓与角色动作后，可使用 [播放记录模板](../assets/visual-review.json) 保存结果：

- 从本次 report.json 复制 planSha256、assetSha256、sourceSha256，填写 actionId。
- 记录实际观察，已完成的八项 checks 才设为 true；全部合适时 verdict 设为 pass。
- evidence 填本次播放说明或截图的相对路径及 SHA256；文件须非空、位于记录目录内。
- 文件指纹可用 PowerShell Get-FileHash 获取，或使用系统的 SHA256 工具。

```text
python <skill-dir>/scripts/verify_action.py <action-dir>/plan.json <action-dir>/runs/r001 --review <action-dir>/visual-review.json
```

返回码 0 表示文件与记录匹配，1 表示需要更新。visualEvidenceCurrent 只表示播放记录对应当前版本；软件适配仍要在目标播放器中试用。工具不会自动填写视觉或运行结果。

静态 PNG 不走 GIF 编译与播放记录。直接检查文件、实际尺寸、轮廓和透明效果，保存在设计简报的完成说明里。

## 成套动作切换

为同一皮肤保持一致的身体显示尺度、装扮和相机。不同分辨率源图可用不同 scale，目标是显示效果一致；特效外框不代表身体大小。

检查实际支持的待机→互动→待机、休息→唤醒，以及可中断动作：身体不意外跳位、配件前后对应、上段特效自然结束。将当前版本、显示尺寸和需要调整的地方记入设计简报。共享定稿或参与动作改变后，再看对应切换。
