# MSearch

Linux 下类似 Everything 的文件名快速检索桌面工具。基于 **Qt 5.12+**，面向麒麟等发行版（含 **ARM64**）。

## 功能

- 指定目录递归扫描，建立内存文件名索引
- 边输入边搜索：子串、通配符、多关键词 AND、`-排除`
- 高级语法：`ext:` / `path:` / `regex:` / `size:` / `dm:`（F1 查看帮助）
- 拼音首字母匹配；过滤器预设与查询书签
- 结果表可按名称 / 路径 / 大小 / 修改时间排序
- 过滤：全部 / 仅文件 / 仅文件夹；区分大小写
- 排除规则（通配）、跳过隐藏文件、符号链接策略
- 可跳过网络盘 / 只读挂载
- **文件系统监控增量更新**（Linux inotify / 跨平台 `QFileSystemWatcher`）
- 系统托盘常驻；关闭可最小化到托盘；全局热键呼出
- 开机自启；索引异步加载与损坏自动重建
- 结果上限可配置；索引进度显示速度
- 搜索历史补全；快捷键 `Ctrl+L` / `Esc` / `F5` / `F1`
- 索引持久化（下次启动秒开）
- 双击打开、右键打开所在目录 / 复制路径
- `.deb` 打包与桌面入口（见 `packaging/`）

## Linux 额外依赖（全局热键）

可选，用于 X11 全局热键：

```bash
sudo apt-get install libqt5x11extras5-dev libx11-dev libxcb1-dev
```

无 X11Extras 时仍可编译运行，仅全局热键不可用（托盘与其它功能正常）。

## 依赖

- CMake ≥ 3.10
- Qt 5.12.8（或同系列 5.12.x）：`Core` / `Gui` / `Widgets`（测试需 `Test`）
- C++14 编译器（`g++` / `clang++`）

麒麟 ARM64 示例（包名因版本可能略有差异）：

```bash
sudo apt-get install build-essential cmake \
  qtbase5-dev qt5-qmake \
  libqt5x11extras5-dev libx11-dev libxcb1-dev
```

## 编译

```bash
cd MSearch
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
ctest --output-on-failure
```

运行：

```bash
./msearch
```

### 交叉编译到 aarch64（在 x86_64 主机）

使用仓库内模板：

```bash
cmake -S . -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-kylin-toolchain.cmake \
  -DCMAKE_PREFIX_PATH=/path/to/qt-5.12.8-aarch64
cmake --build build-aarch64 -j$(nproc)
```

### 打 deb 包

```bash
./packaging/deb/build-deb.sh
```

交付与权限说明见 [docs/PACKAGING.md](docs/PACKAGING.md)。

## 数据位置

- 配置：`QSettings` → 组织/应用名 `MSearch`
- 索引文件：`~/.local/share/MSearch/index.msdb`（依 `AppDataLocation`）

默认首次启动会索引用户主目录。

## 目录结构

```
src/
  app/       主窗口、设置、热键、自启
  index/     条目、内存库、扫描、监控、挂载策略
  search/    检索引擎、查询解析、拼音
  model/     结果表模型
packaging/   desktop、图标、deb 脚本
cmake/       aarch64 工具链模板
tests/       单元测试
docs/        交付说明
```

## 后续规划

详见 [ROADMAP.md](ROADMAP.md)。阶段 0–5 主体已完成；建议在麒麟 ARM64 实机再验证编译与安装。
