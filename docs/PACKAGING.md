# 交付说明（麒麟 / ARM64）

## 权限与索引范围

- **默认**：索引用户主目录（无需 root）。
- **全盘索引**（如 `/`）：需要有权读对应目录；建议用普通用户可读路径，或单独添加 `/home`、`/opt`、`/usr/share` 等。
- 不要默认索引 `/proc` `/sys` `/dev` `/run`（程序已内置跳过）。
- 网络盘、只读盘可在设置中勾选跳过，避免卡顿或无意义条目。

## 在 aarch64 实机编译（推荐）

```bash
sudo apt-get install build-essential cmake \
  qtbase5-dev qt5-qmake \
  libqt5x11extras5-dev libx11-dev libxcb1-dev

git clone https://github.com/manxisuo/msearch.git
cd msearch
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
ctest --output-on-failure
./msearch
```

确认 Qt 为 **5.12.x**（`qmake -query QT_VERSION`）。

## 交叉编译

见仓库根目录 `cmake/aarch64-kylin-toolchain.cmake`。需自备 aarch64 Qt 5.12.8 与 sysroot。

## 打 .deb 包

在已编译出 `build/msearch` 的机器上：

```bash
chmod +x packaging/deb/build-deb.sh
./packaging/deb/build-deb.sh
# 生成 packaging/deb/msearch_<ver>_<arch>.deb
sudo dpkg -i packaging/deb/msearch_*.deb
```

安装后桌面菜单出现「MSearch 文件搜索」。

## 性能基线（自测参考）

在目标机上重建索引后，可用状态栏观察「条/秒」。建议记录：

| 指标 | 参考目标 |
|------|----------|
| 家目录索引条目 | 视用户而定 |
| 输入到首屏结果 | < 100ms（万级～十万级内存索引） |
| 常驻内存 | 与条目数近似线性；百万级建议观察 RSS |

百万级压力可用临时目录生成大量空文件后加入索引路径自测（勿在系统盘无节制生成）。
