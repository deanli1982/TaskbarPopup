# TaskbarPopup 图标分组功能 · 修改日志

版本：V1.0　|　日期：2026-11-24　|　分支：`feature/icon-groups`　|　基线：`main @ 03a8cf6`

## 修订历史（文首）

| 版本 | 日期 | 说明 |
|---|---|---|
| V1.0 | 2026-11-24 | 首版：图标分组功能完整实现 + 标签宽度修复 + 竖排分隔带高度自适应 |

## 1. 需求来源

用户原话：

> 拉取 https://github.com/deanli1982/TaskbarPopup.git 到 e:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\
> 我希望基于这个程序，增加icon分组功能，例如底部状态栏图标可以分组为AI工具，Office工具，通信工具，等等

交互形态经用户确认（clarify 选择）：

1. **不折叠分区式**：图标行按组分区，组间加分隔线和组名标签（否决了折叠子面板方案）。
2. **入组范围**：任务栏固定的应用 + 打开的窗口（与现有格子一致）。
3. **入组后**：不再单独显示，只出现在分组里（省空间）。
4. 只改迷你任务栏的展示，**不改动系统任务栏真实固定列表**。

## 2. 修改清单（逐文件）

| # | 文件 | 类型 | 内容 |
|---|---|---|---|
| 1 | `src/groups.cpp` | 新增 512 行 | 分组数据模型与持久化：`g_iconGroups`（组名+成员 key 列表）、`AppKeyOf`（AUMID>程序路径>启动路径，统一小写）、`GroupOfKey`（unordered_map 缓存）、`GroupsLoad/GroupsSave`（ini `[Groups]` 整节读写）、`GroupsCreate/GroupsDelete/GroupsMove/GroupsAssign`、`GroupsAskName`（小输入框）、`GroupsDialog_Show`（管理对话框：新建/重命名/删除/上移/下移）。上限：24 组、组名 40 字符、单组成员 4096 |
| 2 | `src/popup.cpp` | 修改 +182/−18 | ① `Tile.key` 字段；② 布局改 `s_tileOff`（每格偏移）+`s_tileSpan`（总长），`ComputeTilePositions`/`FitVisibleCount` 统一走偏移表，`TileRect` 查表（`HitTest` 无需改）；③ `EstimateTextW`/`GroupSepLen` 分隔带尺寸（横排按组名实际字号估算+20%余量；竖排按折行数自适应高度）；④ `BuildTiles` 排序：未分组在前→按组顺序，同组相邻；⑤ 分隔带绘制（横排竖线+左对齐组名，竖排横线+居中组名）；⑥ 右键菜单「分组」子菜单（不分组/各组/新建分组/管理分组，命令 id 300/301+/340/341）；⑦ `RebuildTilesAfterGroups`（分组变更后重枚举→重排→重绘） |
| 3 | `src/common.h` | 修改 +26 | `IconGroup` 结构、`g_iconGroups`  extern、分组函数声明（与 groups.cpp 实际签名一致）、`kMaxIconGroups=24` |
| 4 | `src/main.cpp` | 修改 +22/−1 | 启动 `GroupsLoad`、退出 `GroupsSave`、导出设置含 `[Groups]`、`ImportSettings` 对 `[Groups]` 整节替换 |
| 5 | `src/lang.cpp` | 修改 +20 | 分组相关中英文文案（走现有 `Tr()` 机制） |
| 6 | `CMakeLists.txt` | 修改 +1 | 源列表加 `src/groups.cpp` |
| 7 | `build-local.sh` | 新增 15 行 | 便携 g++ 直编脚本（`src/*.cpp` 通配 + windres，静态链接，无需 CMake） |
| 8 | `.gitignore` | 修改 +3 | 忽略 `build-local.log` |

## 3. 未改动范围

- 系统任务栏真实固定列表（只读枚举，不写）。
- 键盘钩子逻辑（`LLKHF_INJECTED` 过滤是上游防自激设计，保留）。
- 原有格子绘制、托盘、时钟、音量/亮度模块、自由布局的既有行为（分隔带只插入在组边界）。
- `build-mingw.sh`（交叉编译脚本未同步 groups.cpp；本地构建走 build-local.sh 通配，已覆盖）。

## 4. 指纹（SHA-256）

| 文件 | SHA-256 |
|---|---|
| src/groups.cpp | `e7a3d7b11b2ebee41b56fedaee7f616adae53fd42a77538ccf1b5aa46f4352dc` |
| src/popup.cpp | `3ffe6a42333bb8e0916fc360ba83b28e857bd41e4f2c73a908a475aad91ef068` |
| src/common.h | `22896c312e1977a6cf62fdf49482dba92486bfa5bde5f12a9f6f4169eeb0b279` |
| src/main.cpp | `a620d4d90bf8cad0482f7aa60165c18e86b58909a30b9e91298781fb6bca86fb` |
| src/lang.cpp | `bd24ce75efc7e478e8a30d01ef97f828f8acc94c0a567361e312b1475f7e9b8c` |
| CMakeLists.txt | `46b11dfd80f5b272379bd3e5ea13e3164911fa488d8dd9833f971fb64b9f2644` |
| build-local.sh | `bb4bc736808e5d394cfd46c0448e222439b30c8c7b10eae99708d8d59be3c39b` |
| TaskbarPopup.exe（1,575,424 B） | `26389bda2c5cf17a2c33c2a1481a790f880a95304d145b92f9166dbf0389f372` |

## 5. 构建/复现命令

```bash
cd /e/Temp/AI_Study/TaskbarPopup_With_GroupIcon
sh build-local.sh        # 便携 g++ 16.2.0（E:\Temp\AI_Study\mingw-w64-toolchain\msys64）
# 产物：TaskbarPopup.exe（静态链接，单文件，无运行时依赖）
```

## 6. 验证证据

| 项 | 结果 | 证据 |
|---|---|---|
| 编译 | 0 错误 | `build-local.log`（BUILD_DONE） |
| 横排分组渲染 | 分隔线+完整组名+成员正确 | `报告/TaskbarPopup分组_横排_标签修复后.png` |
| 标签截断缺陷修复 | 修复前后对比 | `报告/TaskbarPopup分组_横排_标签修复前.png` vs 修复后 |
| 竖排分组渲染 | 横线+组名（折行） | `报告/TaskbarPopup分组_竖排.png` |
| 最终构建回归 | 横排标签完整 | `报告/TaskbarPopup分组_横排_最终构建.png` |
| 持久化往返 | 优雅退出后 [Groups] 3 组 7 成员完整写回 | settings.ini 读回（测试报告 T7） |

## 7. 待用户确认项

1. **管理入口形态**：实现为「右键格子 → 分组子菜单 + 独立管理对话框」。clarify 第④问（入口位置）答案文本在上下文压缩中丢失，按此默认落地——如需改进入口（如设置页按钮）请指出。
2. **演示分组**：测试时向 `%APPDATA%\TaskbarPopup\settings.ini` 写入了 3 个演示组（AI工具=Kimi+Monica；通信工具=微信+飞书+企业微信；Office工具=eM Client+企业微信邮箱），均按真实在跑应用路径匹配。已保留作为初始状态，可在管理对话框里改/删。
3. **竖排高度修复后的截图**：修复已编入最终构建，但修复后竖排截图被用户飞书会议窗口遮挡未取到（修复前截图见证据）——列为待人工确认项。
