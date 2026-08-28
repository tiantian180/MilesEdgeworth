# 桌宠皮肤工坊

为猫、狗、鸟、鱼及其他宠物设计主题装扮、创意道具和连续动作，也适合顾客定制与批量制作。

## 能做什么

从照片或已有角色开始，完成角色定稿、整套表演设计、透明 PNG、连续 GIF，以及目标软件支持的皮肤包。支持只做静态设计或只交付素材，不要求修改应用代码。

这是一套制作工作流和本地工具，不包含图像模型、第三方服务账号或现成商业皮肤。素材生成需要宿主环境提供相应图像工具。

## 使用

将完整的 `desktop-pet-skin-factory` 文件夹放入 Codex 的 skills 目录（默认 `~/.codex/skills/`，设置了 CODEX_HOME 时使用其下的 `skills/`）。已有同名目录时先备份，再用完整新版替换，避免混用文件。

可以这样开始：

> 用 $desktop-pet-skin-factory 根据这张宠物照片做一套星际探险皮肤。保留它的特征，先给我完整装扮和四种动作设计。

> 用 $desktop-pet-skin-factory 为这只鸟设计云端茶馆主题：展开茶帆、接住露珠、送出茶杯。先制作一段连续 GIF 样板。

> 用 $desktop-pet-skin-factory 制作顾客定制皮肤，只交付透明 PNG，资料与成品都保存在该订单的私有目录。

## 本地工具

使用 Python 3.11 和 Pillow 12.3.0 测试。依赖固定在 requirements.txt；其他版本尚未测试。在 skill 目录运行：

```text
python -m pip install -r requirements.txt
python -B -m unittest discover -s scripts -p "test_*.py"
```

测试自带临时样例，不需要宠物素材、API 密钥或应用仓库。安装依赖前也可以先确认当前环境已有相同版本。

| 工具 | 用途 |
| --- | --- |
| scripts/compile_action.py | 按计划裁分、定位透明 PNG，导出连续 GIF 与浅深底预览 |
| scripts/verify_action.py | 检查原图、计划、成片和播放记录是否对应同一版本 |
| scripts/check_skin.py | 检查 DesktopCat 格式皮肤包的文件引用、目录和 PNG/GIF |

详细命令见 [帧编排与导出](references/frame-pipeline.md) 和 [软件接入](references/integration.md)。工具不自动生成姿势、去背景或安装皮肤。

## 文件与交付

图像和 GIF 分目录保存，定稿、源图和提示词留在制作目录，交付目录只包含选定成片。顾客参考与私有成品不加入公开发布包；源文件交付、展示和分发范围按订单约定。

生成工具及输出素材的使用条件由所用服务和素材来源决定。参考项目列于 [sources.md](references/sources.md)；本包不附带它们的代码或游戏素材。
