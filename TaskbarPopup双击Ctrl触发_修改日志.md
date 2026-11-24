# TaskbarPopup 双击 Ctrl 触发 — 修改日志

- 分支：`feature/icon-groups`（基线 `1549073` 图标分组）
- 日期：2026-11-24
- 需求：把「长按 Win 键约 1 秒松开」唤起迷你任务栏改为「双击 Ctrl」
- 改动规模：9 文件，+131 / −76（`git diff --numstat`）

## 1. 逐文件修改

| 文件 | +/− | 修改内容 |
| --- | --- | --- |
| `src/hook.cpp` | +56/−10 | 新增双击 Ctrl 检测（`DoubleTapCtrl`/`DoubleTapReset` + 4 个状态变量）；`HandleKey` 开头接入（只观察不吞键）；`HandleMouse` 非移动事件作废计数；原 Win 长按弹出机制改为仅在「隐藏系统任务栏」模式保留（Win 转发/开始菜单兼容），正常模式 Win 键完全交还系统 |
| `src/common.h` | +8/−4 | 新增 `kMinDoubleTapMs=150` / `kMaxDoubleTapMs=1000` / `kDefaultDoubleTapMs=400`；设置字段 `longPressPopup/longPressMs` → `doubleTapPopup/doubleTapMs`；`WM_APP_LONGPRESS` 注释更新（消息 id 不变，复用弹出通路）；管理员运行注释同步 |
| `src/main.cpp` | +17/−17 | 配置读写：开关沿用 ini 键 `LongPressPopup`（代码字段更名 `doubleTapPopup`，旧设置无缝继承）；间隔改用新键 `DoubleTapMs`（默认 400，钳制 150–1000），旧 `LongPressMs` 不再读取（语义已从长按时长变为双击间隔）；`Hook_Configure` 传参、托盘提示/菜单文案同步 |
| `src/settings_dialog.cpp` | +19/−15 | 开关文案「长按 Win 键弹出迷你任务栏」→「双击 Ctrl 弹出迷你任务栏」；弹出滑条「长按时长 200–5000ms 步进100」→「双击间隔 150–1000ms 步进50」；**新增对称规则**：Apply 时打开双击开关自动清掉「弹出迷你任务栏」快捷键绑定（与原有「录了弹出快捷键就关双击」对称，避免两个触发器打架）；6 个可配置快捷键的长按时长滑条（200–5000ms）不受影响 |
| `src/lang.cpp` | +8/−7 | 中英文条目同步：开关、托盘提示、钩子失败提示、管理员运行、全屏隐藏副标题；新增「双击间隔 / Double-tap gap」 |
| `src/elevation.cpp` | +4/−4 | 管理员运行相关文案「长按 Win」→「双击 Ctrl」 |
| `src/fullscreen.cpp` | +1/−1 | 头注释同步 |
| `README.md` | +9/−9 | 触发方式全部改为双击 Ctrl（简介/功能节/操作表/已知限制/源码表），说明间隔默认 0.4s 可调 150–1000ms、Ctrl 组合键不受影响、Win 键交还系统 |
| `README.en.md` | +9/−9 | 同上英文 |

## 2. 双击判定规则（`src/hook.cpp` DoubleTapCtrl）

1. 只认左右 Ctrl（`VK_LCONTROL`/`VK_RCONTROL`），**只观察不吞键**，单独轻点 Ctrl 对系统无副作用。
2. 一次「干净轻点」= 按下→松开，期间：
   - 没先按住 Shift/Alt/Win（`GetAsyncKeyState` 判定；不查 Ctrl 自身，避免按下瞬间异步状态已含本次按键的时序歧义）；
   - 按住时长 ≤ `kTapMaxMs`（600ms，复用现有常量）；
   - 自动重复（按住不放）不算新按下。
3. 两下干净轻点的**松开→松开间隔** ≤ `doubleTapMs`（默认 400，设置可调 150–1000）→ `PostMessageW(g_mainWnd, WM_APP_LONGPRESS)` 弹出/收起（toggle，与原长按同一通路）。
4. 作废条件：两下之间按了别的键、点鼠标/滚轮（`HandleMouse`）、轻点不干净、超时。因此 **Ctrl+C、Ctrl+点选、Ctrl+Shift 切输入法、Ctrl+滚轮缩放都不会误触发**。
5. 程序注入的按键（`LLKHF_INJECTED`）不参与判定，与快捷键钩子同一规矩。

## 3. 兼容与迁移

- **Win 键**：正常模式下钩子不再拦截任何 Win 事件，开始菜单/Win 组合键完全原生。「隐藏系统任务栏」模式下保留原 Win 长按/短按转发逻辑（该模式需要 Win 语义唤起）。
- **旧配置**：开关键 `LongPressPopup` 沿用，旧设置无缝继承；间隔读新键 `DoubleTapMs`（缺省 400），旧 `LongPressMs` 值忽略（长按时长语义不再适用）。
- **本机设置迁移**（用户原配置 `PopupHotkey=LCtrl` + `PopupHotkeyHold=1` 长按 500ms，系此前用可配置快捷键模拟 Ctrl 唤起）：已置 `LongPressPopup=1`（启用双击）、清空 `PopupHotkey`、`PopupHotkeyHold=0`、写入 `DoubleTapMs=400`。
- 消息 id `WM_APP_LONGPRESS = WM_APP+2` 与自测 toggle `0x8006` 不变。

## 4. 构建

- 工具链：便携 MSYS2 mingw-w64 g++ 16.2.0，`sh build-local.sh`
- 最终产物：`E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\TaskbarPopup.exe`，1,576,448 字节，2026-11-24 18:47，0 error / 0 warning