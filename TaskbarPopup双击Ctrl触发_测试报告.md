# TaskbarPopup 双击 Ctrl 触发 — 测试报告

- 分支：`feature/icon-groups`，日期：2026-11-24
- 被测产物：`E:\Temp\AI_Study\TaskbarPopup_With_GroupIcon\TaskbarPopup.exe`（1,576,448 B，18:47 最终版）
- 工具链：便携 MSYS2 mingw-w64 g++ 16.2.0，`sh build-local.sh`

## 1. 编译

| 项 | 结果 |
| --- | --- |
| 最终构建 | ✅ 0 error / 0 warning |
| 产物 | TaskbarPopup.exe 1,576,448 B（2026-11-24 18:47） |
| 插桩残留检查 | ✅ exe 内无 `DT entry` 等测试日志串；源码无 `TESTBUILD`/`DT ` 残留 |

## 2. 检测逻辑实测（7/7 通过）

方法：临时测试版仅放开 `DoubleTapCtrl` 的 `LLKHF_INJECTED` 过滤（其余代码与生产一致），用 **扫描码式 SendInput**（`KEYEVENTF_SCANCODE`，左 Ctrl 扫描码 0x1D、右 Ctrl 加扩展标志、C 键 0x2E）驱动真实钩子路径，检查 `TaskbarPopupMini` 窗口可见性。测毕还原注入过滤并重编。

| # | 场景 | 期望 | 结果 |
| --- | --- | --- | --- |
| T1 | 双击 Ctrl（80ms 轻点 ×2，间隔 150ms） | 弹出 | ✅ PASS（首次即过） |
| T2 | 单击 Ctrl | 不弹 | ✅ PASS |
| T3 | Ctrl+C | 不弹 | ✅ PASS |
| T4 | 慢速两下（间隔 1.2s > 400ms） | 不弹 | ✅ PASS |
| T5 | 每下按住 0.8s（> 600ms 轻点上限）×2 | 不弹 | ✅ PASS |
| T6 | 弹出状态下再双击 | 收起（toggle） | ✅ PASS |
| T7 | 右 Ctrl 双击 | 弹出 | ✅ PASS |

脚本：`C:\Users\Lenovo\AppData\Local\hermes\cache\scratch\test_doubletap3.py`

## 3. 生产版验证

| 项 | 结果 |
| --- | --- |
| 注入过滤恢复 | ✅ 生产版扫描码双击 Ctrl **不触发**（`LLKHF_INJECTED` 过滤生效，与快捷键钩子同规矩） |
| 弹出链路 sanity | ✅ `PostMessageW(main, 0x8006)` 弹出/收起正常（最终版 pid 51704） |
| 设置迁移 | ✅ `LongPressPopup=1`、`PopupHotkey` 清空、`PopupHotkeyHold=0`、`DoubleTapMs=400`；`DebugLog` 恢复 0 |
| 6 个快捷键长按时长滑条 | ✅ 仍用 `kMinLongPressMs/kMaxLongPressMs`（200–5000ms），未被双击常量误改（git diff 核实仅 1 处滑条行变更） |
| 文案残留扫描 | ✅ src/ 与中英文 README 无「长按 Win / Hold Win」残留（隐藏任务栏模式的 Win 转发描述按设计保留） |

## 4. 测试环境坑（本机特有，记录备查）

1. **VK 式 SendInput 被拦**：`wVk=0xA2` 的 SendInput 返回 0（本机安全策略）；**扫描码式**（`wVk=0, wScan=0x1D, KEYEVENTF_SCANCODE`）与 `keybd_event` 正常返回 1。自动化测试须用扫描码式。
2. **前台锁定**：前台为卡死/受保护窗口时 SetForegroundWindow 失败；`AttachThreadInput` + `BringWindowToTop` + `SetFocus` 可取得前台。SPI 前台锁定超时被策略固定为 0x7FFFFFFF，无法临时修改。
3. **read_file 通道对 src/hook.cpp 失真**：一律以 python `io.open` / `sed` / `git show` 磁盘原文为锚点。
4. exe 被运行实例占用 → 链接 `Permission denied`；先 `OpenProcess(1)+TerminateProcess` 再编。

## 5. 待用户人工确认（无法注入物理按键）

- [ ] 物理双击左/右 Ctrl 弹出迷你任务栏（间隔默认 0.4s，设置 → 弹出 →「双击间隔」150–1000ms 可调）
- [ ] Win 键开始菜单、Win 组合键完全原生
- [ ] Ctrl+C / Ctrl+Shift 切输入法 / Ctrl+滚轮缩放不误触发
- [ ] 设置里打开「双击 Ctrl」开关时，原「弹出迷你任务栏」快捷键绑定自动清空（对称规则）
- [ ] 管理员权限程序前台时双击 Ctrl 的行为（UIPI 限制，需勾选「以管理员身份运行」，与原长按 Win 相同）