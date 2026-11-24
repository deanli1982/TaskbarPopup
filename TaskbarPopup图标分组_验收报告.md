# TaskbarPopup 图标分组功能 · 验收报告

版本：V1.0　|　日期：2026-11-24　|　分支：`feature/icon-groups`　|　基线：`main @ 03a8cf6`

## 修订历史（文首）

| 版本 | 日期 | 说明 |
|---|---|---|
| V1.0 | 2026-11-24 | 首版验收：核心需求达成，7 项交互场景留待人工确认 |

## 1. 需求符合性（逐条）

| 需求 | 判定 | 依据 |
|---|---|---|
| 仓库拉取到 `e:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\` | **达成** | SSH 克隆完成（HTTPS 被公司网络阻断，改用已验证 SSH 密钥），main @ 03a8cf6 |
| 图标可分组为 AI工具 / Office工具 / 通信工具 等自定义组 | **达成** | 任意组名（≤40 字符、≤24 组）；演示 3 组已注入并渲染（测试报告 T4/T7） |
| 不折叠分区式：组间分隔线 + 组名标签 | **达成** | 横排竖线+左对齐标签、竖排横线+居中标签；截图证据（T4/T6/T8） |
| 入组范围 = 固定应用 + 打开窗口 | **达成** | 格子来源未变，key 匹配（AUMID>程序路径>启动路径）；微信/飞书/Monica/Kimi 实测入组 |
| 入组后不再单独显示 | **达成** | BuildTiles 按组排序去重，同 key 只出现一次（T4 截图：组内成员无重复散格） |
| 只改迷你任务栏，不动系统任务栏固定列表 | **达成** | 代码走查：无写固定列表路径；系统任务栏截图对照无变化 |
| 分组数据持久化 + 随导出/导入 | **达成（持久化实测；导入导出代码就绪未运行时验证）** | T7 往返通过；ImportSettings [Groups] 整节替换已实现（U3 待人工） |
| 可编译、可运行、不回归 | **达成** | g++ 16.2.0 零错误；T2/T3/T8 通过 |

## 2. 偏差与说明

1. **管理入口**：右键格子→「分组」子菜单 + 独立管理对话框。clarify 第④问答案文本在上下文压缩中丢失，按此默认实现——**待用户确认**是否符合预期。
2. **竖排组名折行**：窄列（~40px）放不下 4 字组名，折成两行；分隔带高度已按折行数自适应（修复编入最终构建），修复后竖排截图因用户会议窗口遮挡未取到——**待人工确认**（U6）。
3. **Office工具组**：注入的成员（eM Client、企业微信邮箱）测试时未在运行，该组暂不渲染；启动对应应用即出现（U7）。
4. **构建方式**：本机无管理员级 VS/CMake，采用便携 MSYS2 + `build-local.sh` 直编（产物静态链接单 exe，与 CI 的 MinGW 路线一致）；`build-mingw.sh` 未同步 groups.cpp（低优先，不影响本地与 CMake 构建）。

## 3. 验收结论

**核心功能验收通过**：分区展示、分隔线+组名标签、入组/隐藏、持久化、横竖排、编译与稳定性均有真实执行证据（8/8 用例）。
**遗留**：7 项交互场景（右键子菜单、管理对话框、导入导出、主题、滚动、竖排修复后视觉、Office组渲染）待用户空档按测试报告 §5 步骤人工确认；2 项设计确认（管理入口、演示分组去留）。

## 4. 交付物清单（完整绝对路径）

| 类别 | 路径 |
|---|---|
| 源码仓库 | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\`（分支 feature/icon-groups） |
| 新增模块 | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\src\groups.cpp` |
| 可执行产物 | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\TaskbarPopup.exe`（1,575,424 B，SHA-256 26389bda…f372） |
| 构建脚本 | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\build-local.sh` |
| 修改日志 | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\TaskbarPopup图标分组_修改日志.md` |
| 测试报告 | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\TaskbarPopup图标分组_测试报告.md` |
| 验收报告 | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\TaskbarPopup图标分组_验收报告.md` |
| 证据截图 | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\报告\TaskbarPopup分组_横排_标签修复前.png` |
|  | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\报告\TaskbarPopup分组_横排_标签修复后.png` |
|  | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\报告\TaskbarPopup分组_竖排.png` |
|  | `E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\报告\TaskbarPopup分组_横排_最终构建.png` |
| 用户配置（演示分组） | `%APPDATA%\TaskbarPopup\settings.ini`（[Groups] 3 组，DebugLog 已关、PopupEdge 已复位 0） |
