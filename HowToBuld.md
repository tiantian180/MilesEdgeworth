# 命令行编译

## 安装cmake

## 构建

### DCMAKE_PREFIX_PATH的值改为本地qt实际位置

``` sh
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH=C:/Qt/6.5.3/msvc2019_64/lib/cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE
```
## 编译

```sh
cmake --build build --config=Release
```
# vscode插件编译

## 安装cmake

## 项目目录下新建.vscode文件夹

## 添加launch.json文件

```json
{
    "cmake.configureOnOpen": false,
    "diffEditor.ignoreTrimWhitespace": false,
    "cmake.buildDirectory": "${workspaceFolder}/${variant:buildType}",
    "cmake.buildBeforeRun": false,
    "cmake.generator": "Visual Studio 17 2022",
    "cmake.configureSettings": {
        "CMAKE_PREFIX_PATH": "C:/Qt/6.5.3/msvc2019_64/lib/cmake", //改为本地qt实际位置
		"CMAKE_BUILD_TYPE": "${variant:buildType}"
    }
}
```

## 添加cmake-variants.yaml文件

```yaml
buildType:
  default: Debug
  choices:
    debug:
      short: Debug
      long: Emit debug information
      buildType: Debug
    release:
      short: Release
      long: Optimize generated code
      buildType: Release

```

## 通过cmake提供的快捷按钮构建并编译

# 打包安装

```sh
cd build
cmake --install .
cpack
```

​	
