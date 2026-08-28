# 动作制作

从完整身体动作出发，让道具和特效跟随角色，而不是围绕静态形象旋转。

## 分镜

每段写清准备、发力、反馈与收势。标出身体轨迹、参与部位、接触时刻，以及道具在每一拍的状态。循环只包含稳定周期，登场和退出另行安排。

以四足宠物扑球为例：重心后移→蓄力→沿弧线跃起→接触球→球回弹→身体缓冲→站稳。其他物种按自身结构重新设计，不直接套用这条动作。

预备和极限姿态可略长，快速经过区较短，收势留出理解动作的时间。衣物、附肢和粒子的滞后要有原因。休息采用适合该角色的安静周期，骑乘循环保持乘坐状态。

## 关键姿势到过渡

1. 为决定表演的姿势命名，例如准备、接触、峰值和收势；数量由动作决定。
2. 使用同一角色定稿和装扮参考生成关键姿势。小姿态板便于比较；拥挤或串格时拆成短段或单帧。
3. 看清完整身体、部位归属、标记和配件连接。透视与遮挡可以变化，身份和装扮保持一致。
4. 以选定关键姿势为两端，结合相邻姿态补中间帧。重点补接触、转向和快速运动区，不平均凑帧数。
5. 保留选定端点；若需要修改关键姿势，同时调整相邻过渡，再播放整段。

原图本身不连贯时补真实姿势，交叉淡化、光流或剪下肢体旋转不适合作为结构修复。叙事动作按时间推进，往返播放只用于本来就往复的运动。

## 生成提示词

按本段选择字段，图像工具的调用方式遵循当前环境。参考图片需实际传入工具；无法使用参考时先说明对角色一致性的影响。

```text
Create production animation poses for this individual pet.
Identity: [reference image, silhouette, markings, proportions, rendering style].
Structure: [actual body parts, counts, materials and movement mode].
Outfit: [selected design, attachments, colors and recurring props].
Action: [preparation, trajectory, contact, peak, recovery].
Effects: [origin, main shape, depth order, timing and dissipation].
Pose references: [selected key poses; interval being completed].
Change only: [this pose or local correction]; preserve the selected design.
Layout: [N chronological poses, columns/rows or individual frames].
Keep camera and scale consistent. Preserve planned movement within each cell.
Keep the full pet, appendages, props and effects inside clear cell margins.
Output true transparent alpha PNG, without labels, grid lines or painted checkerboards.
Loop or ending: [actual start/end state].
```

固定画布参考用于保留移动；只有原地表演才追踪身体中心重新对齐。不要逐帧按角色包围盒缩放。格内参考如何换算为原图坐标，见 [帧编排与导出](frame-pipeline.md)。

## 透明素材

先用有代表性的毛羽边缘、薄鳍、装扮或独立特效试一帧，再完成短段。查看实际尺寸与 alpha，而不只依赖提示词声明。

如果返回 RGB 或画出来的棋盘格，用已获准的图像编辑工具重新处理背景。处理后同时查看身份、比例、轮廓和独立道具；不要按浅色删除像素，把白毛或高光一并擦掉。

GIF 只有二值透明。保留原始 RGBA PNG，用清楚的特效核心和轮廓设计动画，在浅深底查看最终 GIF 的光晕与细节。样板效果合适后再扩展，不以其他格式的透明效果代替 GIF 预览。

## 播放与调整

先看实际桌宠大小，再看放大细节；按原速、慢速和首尾连接查看动作。确认主体连续、配件稳定、道具完整、上一帧没有残影。编码帧数不等于独立姿势数量。

只修问题帧或区间，保留已经合适的内容。若整体轨迹不对，回到分镜；若只有导出后才出问题，先检查定位、透明阈值与编码。相同问题连续两次定向修改未改善时，与用户确定下一步方案。
