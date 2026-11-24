// 图标分组：把迷你任务栏的格子按应用分成若干区（AI 工具、Office 工具、通信工具……），
// 区与区之间画分隔线 + 组名。只改迷你任务栏的展示，不动系统任务栏的固定列表。
//
// 数据放在 settings.ini 的 [Groups] 节，随「导出/导入设置」一起走：
//   [Groups]
//   Count=2
//   G1Name=AI 工具
//   G1Count=1
//   G1M1=<应用 key>
//   G2Name=Office 工具
//   ...
// 应用 key 与格子/固定应用同一口径：AUMID（小写）> 程序路径（小写）> 启动路径（小写），
// 所以同一程序的多个窗口、固定格子和窗口格子会进同一个组。
#include "common.h"

#include <algorithm>
#include <unordered_map>
#include <dwmapi.h>

namespace app {

std::vector<IconGroup> g_iconGroups;

namespace {

constexpr int kMaxGroupName = 40;      // 组名上限（字符数），菜单/标签都放得下

std::unordered_map<std::wstring, int> s_keyMap;  // 应用 key -> 组序号
bool s_mapValid = false;

void RebuildKeyMap() {
    s_keyMap.clear();
    for (size_t gi = 0; gi < g_iconGroups.size(); ++gi)
        for (const std::wstring& key : g_iconGroups[gi].members)
            s_keyMap.emplace(key, static_cast<int>(gi));
    s_mapValid = true;
}

std::wstring TrimName(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    std::wstring out = s.substr(a, b - a);
    if (out.size() > kMaxGroupName) out.resize(kMaxGroupName);
    return out;
}

bool NameTaken(const std::wstring& name, int exceptIdx) {
    for (size_t i = 0; i < g_iconGroups.size(); ++i)
        if (static_cast<int>(i) != exceptIdx && g_iconGroups[i].name == name) return true;
    return false;
}

// ---------------- 原生模态对话框的小框架 ----------------
// CreateWindowExW + IsDialogMessageW 的手动模态循环：Tab/Enter/Esc 都按对话框的规矩走。
struct DlgState {
    bool done = false;
    bool ok = false;
    bool dark = false;
    int dpi = 96;
    HFONT font = nullptr;
    HBRUSH bgBrush = nullptr;
    HBRUSH editBrush = nullptr;
    std::wstring text;  // 输入对话框的结果
    int sel = -1;       // 管理对话框：列表当前选中项
};

int Scale(DlgState* st, int v) { return MulDiv(v, st->dpi, 96); }

COLORREF BgColor(bool dark) { return dark ? RGB(32, 32, 32) : RGB(250, 250, 250); }
COLORREF FgColor(bool dark) { return dark ? RGB(240, 240, 240) : RGB(20, 20, 20); }
COLORREF EditBg(bool dark) { return dark ? RGB(45, 45, 45) : RGB(255, 255, 255); }

void ApplyDarkTitleBar(HWND hwnd, bool dark) {
    BOOL on = dark;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &on, sizeof(on))))
        DwmSetWindowAttribute(hwnd, 19, &on, sizeof(on));  // 1809~1903 的旧编号
}

HFONT MakeDialogFont(int dpi) {
    return CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
}

void SetChildFonts(HWND hwnd, HFONT font) {
    EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(lp), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(font));
}

// 自绘按钮：深色/浅色都平整，不靠系统按钮的脸
void DrawDialogButton(const DRAWITEMSTRUCT* dis, bool dark) {
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    COLORREF face = dark ? (pressed ? RGB(68, 68, 68) : RGB(52, 52, 52))
                         : (pressed ? RGB(225, 225, 225) : RGB(255, 255, 255));
    COLORREF edge = dark ? RGB(96, 96, 96) : RGB(180, 180, 180);
    HBRUSH br = CreateSolidBrush(face);
    FillRect(dis->hDC, &dis->rcItem, br);
    DeleteObject(br);
    HPEN pen = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
    HGDIOBJ oldBr = SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
    Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
    SelectObject(dis->hDC, oldBr);
    SelectObject(dis->hDC, oldPen);
    DeleteObject(pen);
    wchar_t text[128];
    GetWindowTextW(dis->hwndItem, text, ARRAYSIZE(text));
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, disabled ? (dark ? RGB(120, 120, 120) : RGB(160, 160, 160)) : FgColor(dark));
    RECT rc = dis->rcItem;
    DrawTextW(dis->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (dis->itemState & ODS_FOCUS) {
        RECT r = dis->rcItem;
        InflateRect(&r, -3, -3);
        DrawFocusRect(dis->hDC, &r);
    }
}

LRESULT CALLBACK DlgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DlgState* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_ERASEBKGND: {
            if (!st) break;
            RECT r;
            GetClientRect(hwnd, &r);
            FillRect(reinterpret_cast<HDC>(wp), &r, st->bgBrush);
            return 1;
        }
        case WM_CTLCOLORSTATIC: {
            if (!st) break;
            SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
            SetTextColor(reinterpret_cast<HDC>(wp), FgColor(st->dark));
            return reinterpret_cast<LRESULT>(st->bgBrush);
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            if (!st) break;
            SetBkColor(reinterpret_cast<HDC>(wp), EditBg(st->dark));
            SetTextColor(reinterpret_cast<HDC>(wp), FgColor(st->dark));
            return reinterpret_cast<LRESULT>(st->editBrush);
        }
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
            if (dis->CtlType == ODT_BUTTON && st) {
                DrawDialogButton(dis, st->dark);
                return TRUE;
            }
            break;
        }
        case WM_CLOSE:
            if (st) st->done = true;
            return 0;
        case WM_DESTROY:
            if (st) st->done = true;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

const wchar_t* DlgClassName() {
    static const wchar_t* name = [] {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = DlgWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"TaskbarPopupGroupDlg";
        RegisterClassW(&wc);
        return wc.lpszClassName;
    }();
    return name;
}

HWND CreateDlgWindow(HWND owner, DlgState* st, const wchar_t* title, int clientW, int clientH) {
    st->dark = !ThemeIsLight(g_settings.theme, false);
    st->dpi = owner ? static_cast<int>(GetDpiForWindow(owner)) : static_cast<int>(GetDpiForSystem());
    if (st->dpi < 96) st->dpi = 96;
    st->font = MakeDialogFont(st->dpi);
    st->bgBrush = CreateSolidBrush(BgColor(st->dark));
    st->editBrush = CreateSolidBrush(EditBg(st->dark));
    int w = Scale(st, clientW), h = Scale(st, clientH);
    RECT wr = {0, 0, w, h};
    AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    RECT ar = {};
    if (owner) GetWindowRect(owner, &ar);
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &ar, 0);
    int x = ar.left + ((ar.right - ar.left) - ww) / 2, y = ar.top + ((ar.bottom - ar.top) - wh) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, DlgClassName(), title, WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                x, y, ww, wh, owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) return nullptr;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
    ApplyDarkTitleBar(hwnd, st->dark);
    return hwnd;
}

HWND AddChild(HWND parent, DlgState* st, const wchar_t* cls, const wchar_t* text, DWORD style, int id, int x, int y,
              int w, int h) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, Scale(st, x), Scale(st, y), Scale(st, w),
                             Scale(st, h), parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    if (c) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
    return c;
}

void RunModalLoop(HWND hwnd, HWND owner, DlgState* st) {
    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    MSG msg;
    while (!st->done) {
        BOOL r = GetMessageW(&msg, nullptr, 0, 0);
        if (r <= 0) break;
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
}

void DestroyDlg(HWND hwnd, DlgState* st) {
    DestroyWindow(hwnd);
    if (st->font) DeleteObject(st->font);
    if (st->bgBrush) DeleteObject(st->bgBrush);
    if (st->editBrush) DeleteObject(st->editBrush);
}

// ---------------- 管理对话框的控件编号 ----------------
enum {
    IDC_LIST = 1001,
    IDC_NEW,
    IDC_RENAME,
    IDC_DELETE,
    IDC_UP,
    IDC_DOWN,
    IDC_HINT,
    IDC_EDIT = 1101,
    IDC_PROMPT,
};

void RefreshGroupList(HWND dlg, DlgState* st) {
    HWND list = GetDlgItem(dlg, IDC_LIST);
    int keep = st->sel;
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const IconGroup& g : g_iconGroups) {
        std::wstring line = g.name + L"　·　" + TrF(L"%d 个应用", static_cast<int>(g.members.size()));
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    }
    int n = static_cast<int>(g_iconGroups.size());
    if (keep >= n) keep = n - 1;
    st->sel = keep;
    if (keep >= 0) SendMessageW(list, LB_SETCURSEL, keep, 0);
    EnableWindow(GetDlgItem(dlg, IDC_RENAME), keep >= 0);
    EnableWindow(GetDlgItem(dlg, IDC_DELETE), keep >= 0);
    EnableWindow(GetDlgItem(dlg, IDC_UP), keep > 0);
    EnableWindow(GetDlgItem(dlg, IDC_DOWN), keep >= 0 && keep < n - 1);
    EnableWindow(GetDlgItem(dlg, IDC_NEW), n < kMaxIconGroups);
}

LRESULT CALLBACK ManageWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DlgState* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_COMMAND && st) {
        HWND list = GetDlgItem(hwnd, IDC_LIST);
        int code = HIWORD(wp), id = LOWORD(wp);
        if (id == IDCANCEL) {  // 关闭按钮或 Esc（IsDialogMessage 发来的通知码不一定是 BN_CLICKED）
            st->done = true;
            return 0;
        }
        if (id == IDC_LIST && code == LBN_SELCHANGE) {
            st->sel = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
            RefreshGroupList(hwnd, st);
            return 0;
        }
        if (code == BN_CLICKED) {
            int idx = st->sel;
            int n = static_cast<int>(g_iconGroups.size());
            switch (id) {
                case IDC_NEW: {
                    std::wstring name;
                    if (GroupsAskName(hwnd, Tr(L"新建分组"), name)) {
                        if (GroupsCreate(name)) st->sel = static_cast<int>(g_iconGroups.size()) - 1;
                        else MessageBoxW(hwnd, Tr(L"已有同名分组，或分组数已达上限"), L"TaskbarPopup", MB_ICONINFORMATION);
                        RefreshGroupList(hwnd, st);
                    }
                    return 0;
                }
                case IDC_RENAME: {
                    if (idx < 0 || idx >= n) return 0;
                    std::wstring name = g_iconGroups[idx].name;
                    if (GroupsAskName(hwnd, Tr(L"重命名分组"), name)) {
                        if (!GroupsRename(idx, name))
                            MessageBoxW(hwnd, Tr(L"已有同名分组，或分组数已达上限"), L"TaskbarPopup", MB_ICONINFORMATION);
                        RefreshGroupList(hwnd, st);
                    }
                    return 0;
                }
                case IDC_DELETE: {
                    if (idx < 0 || idx >= n) return 0;
                    std::wstring ask = TrF(L"删除分组「%ls」？里面的应用会回到不分组。", g_iconGroups[idx].name.c_str());
                    if (MessageBoxW(hwnd, ask.c_str(), L"TaskbarPopup", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        GroupsDelete(idx);
                        RefreshGroupList(hwnd, st);
                    }
                    return 0;
                }
                case IDC_UP:
                    if (idx > 0) {
                        GroupsMove(idx, -1);
                        st->sel = idx - 1;
                        RefreshGroupList(hwnd, st);
                    }
                    return 0;
                case IDC_DOWN:
                    if (idx >= 0 && idx < n - 1) {
                        GroupsMove(idx, 1);
                        st->sel = idx + 1;
                        RefreshGroupList(hwnd, st);
                    }
                    return 0;
            }
        }
    }
    return DlgWndProc(hwnd, msg, wp, lp);
}

}  // namespace

// ---------------- 应用 key 与归属 ----------------

std::wstring AppKeyOf(const std::wstring& aumid, const std::wstring& exePath, const std::wstring& launch) {
    if (!aumid.empty()) return ToLower(aumid);
    if (!exePath.empty()) return ToLower(exePath);
    std::wstring l = ToLower(launch);
    const std::wstring prefix = L"shell:appsfolder\\";
    if (l.rfind(prefix, 0) == 0) return l.substr(prefix.size());  // 商店应用：启动路径里就是 AUMID
    return l;
}

int GroupOfKey(const std::wstring& key) {
    if (key.empty()) return -1;
    if (!s_mapValid) RebuildKeyMap();
    auto it = s_keyMap.find(key);
    return it == s_keyMap.end() ? -1 : it->second;
}

void GroupsInvalidate() { s_mapValid = false; }

// ---------------- 持久化 ----------------

void GroupsLoad() {
    g_iconGroups.clear();
    std::wstring f = SettingsFile();
    int count = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Groups", L"Count", 0, f.c_str())), 0, kMaxIconGroups);
    std::vector<wchar_t> buf(32768);
    for (int i = 1; i <= count; ++i) {
        std::wstring pre = L"G" + std::to_wstring(i);
        GetPrivateProfileStringW(L"Groups", (pre + L"Name").c_str(), L"", buf.data(), static_cast<DWORD>(buf.size()),
                                 f.c_str());
        IconGroup g;
        g.name = buf.data();
        if (g.name.empty()) continue;
        int n = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Groups", (pre + L"Count").c_str(), 0, f.c_str())), 0,
                           kMaxMembersPerGroup);
        for (int m = 1; m <= n; ++m) {
            GetPrivateProfileStringW(L"Groups", (pre + L"M" + std::to_wstring(m)).c_str(), L"", buf.data(),
                                     static_cast<DWORD>(buf.size()), f.c_str());
            if (*buf.data()) g.members.push_back(buf.data());
        }
        g_iconGroups.push_back(std::move(g));
    }
    GroupsInvalidate();
}

void GroupsSave() {
    EnsureUnicodeIni();
    std::wstring f = SettingsFile();
    WritePrivateProfileStringW(L"Groups", nullptr, nullptr, f.c_str());  // 整节重写，不留旧键
    WritePrivateProfileStringW(L"Groups", L"Count", std::to_wstring(g_iconGroups.size()).c_str(), f.c_str());
    for (size_t i = 0; i < g_iconGroups.size(); ++i) {
        std::wstring pre = L"G" + std::to_wstring(i + 1);
        WritePrivateProfileStringW(L"Groups", (pre + L"Name").c_str(), g_iconGroups[i].name.c_str(), f.c_str());
        WritePrivateProfileStringW(L"Groups", (pre + L"Count").c_str(), std::to_wstring(g_iconGroups[i].members.size()).c_str(),
                                   f.c_str());
        for (size_t m = 0; m < g_iconGroups[i].members.size(); ++m)
            WritePrivateProfileStringW(L"Groups", (pre + L"M" + std::to_wstring(m + 1)).c_str(),
                                       g_iconGroups[i].members[m].c_str(), f.c_str());
    }
}

// ---------------- 编辑 ----------------

void GroupsAssign(const std::wstring& key, int groupIdx) {
    if (key.empty()) return;
    for (IconGroup& g : g_iconGroups)
        g.members.erase(std::remove(g.members.begin(), g.members.end(), key), g.members.end());
    if (groupIdx >= 0 && groupIdx < static_cast<int>(g_iconGroups.size())) g_iconGroups[groupIdx].members.push_back(key);
    GroupsSave();
    GroupsInvalidate();
}

bool GroupsCreate(const std::wstring& name) {
    std::wstring clean = TrimName(name);
    if (clean.empty() || g_iconGroups.size() >= static_cast<size_t>(kMaxIconGroups) || NameTaken(clean, -1)) return false;
    g_iconGroups.push_back({clean, {}});
    GroupsSave();
    GroupsInvalidate();
    return true;
}

bool GroupsRename(int idx, const std::wstring& name) {
    std::wstring clean = TrimName(name);
    if (idx < 0 || idx >= static_cast<int>(g_iconGroups.size()) || clean.empty() || NameTaken(clean, idx)) return false;
    g_iconGroups[idx].name = clean;
    GroupsSave();
    GroupsInvalidate();
    return true;
}

void GroupsDelete(int idx) {
    if (idx < 0 || idx >= static_cast<int>(g_iconGroups.size())) return;
    g_iconGroups.erase(g_iconGroups.begin() + idx);
    GroupsSave();
    GroupsInvalidate();
}

void GroupsMove(int idx, int delta) {
    int n = static_cast<int>(g_iconGroups.size());
    int to = idx + delta;
    if (idx < 0 || idx >= n || to < 0 || to >= n) return;
    std::swap(g_iconGroups[idx], g_iconGroups[to]);
    GroupsSave();
    GroupsInvalidate();
}

// ---------------- 输入对话框（新建/重命名共用） ----------------

bool GroupsAskName(HWND owner, const wchar_t* title, std::wstring& name) {
    DlgState st;
    st.text = name;
    HWND hwnd = CreateDlgWindow(owner, &st, title, 340, 112);
    if (!hwnd) return false;
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(+[](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
        DlgState* s = reinterpret_cast<DlgState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_COMMAND && s) {
            if (LOWORD(w) == IDOK && HIWORD(w) == BN_CLICKED) {
                wchar_t buf[256];
                GetWindowTextW(GetDlgItem(h, IDC_EDIT), buf, ARRAYSIZE(buf));
                std::wstring clean = TrimName(buf);
                if (clean.empty()) {
                    MessageBeep(MB_ICONWARNING);
                    SetFocus(GetDlgItem(h, IDC_EDIT));
                    return 0;
                }
                s->text = clean;
                s->ok = true;
                s->done = true;
                return 0;
            }
            if (LOWORD(w) == IDCANCEL) {
                s->done = true;
                return 0;
            }
        }
        if (m == WM_SHOWWINDOW && w) {
            HWND edit = GetDlgItem(h, IDC_EDIT);
            SetFocus(edit);
            SendMessageW(edit, EM_SETSEL, 0, -1);
            return 0;
        }
        return DlgWndProc(h, m, w, l);
    }));
    AddChild(hwnd, &st, L"STATIC", Tr(L"分组名"), 0, IDC_PROMPT, 12, 12, 316, 18);
    AddChild(hwnd, &st, L"EDIT", st.text.c_str(), WS_BORDER | ES_AUTOHSCROLL, IDC_EDIT, 12, 34, 316, 24);
    AddChild(hwnd, &st, L"BUTTON", Tr(L"确定"), BS_OWNERDRAW | BS_DEFPUSHBUTTON | WS_TABSTOP, IDOK, 160, 74, 80, 26);
    AddChild(hwnd, &st, L"BUTTON", Tr(L"取消"), BS_OWNERDRAW | WS_TABSTOP, IDCANCEL, 248, 74, 80, 26);
    SetChildFonts(hwnd, st.font);
    RunModalLoop(hwnd, owner, &st);
    bool ok = st.ok;
    if (ok) name = st.text;
    DestroyDlg(hwnd, &st);
    return ok;
}

// ---------------- 管理对话框 ----------------

void GroupsDialog(HWND owner) {
    DlgState st;
    HWND hwnd = CreateDlgWindow(owner, &st, Tr(L"管理分组"), 380, 316);
    if (!hwnd) return;
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ManageWndProc));
    AddChild(hwnd, &st, L"LISTBOX", nullptr, WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                         IDC_LIST, 12, 12, 356, 216);
    AddChild(hwnd, &st, L"BUTTON", Tr(L"新建…"), BS_OWNERDRAW | WS_TABSTOP, IDC_NEW, 12, 238, 80, 28);
    AddChild(hwnd, &st, L"BUTTON", Tr(L"重命名…"), BS_OWNERDRAW | WS_TABSTOP, IDC_RENAME, 100, 238, 80, 28);
    AddChild(hwnd, &st, L"BUTTON", Tr(L"删除"), BS_OWNERDRAW | WS_TABSTOP, IDC_DELETE, 188, 238, 70, 28);
    AddChild(hwnd, &st, L"BUTTON", Tr(L"上移"), BS_OWNERDRAW | WS_TABSTOP, IDC_UP, 266, 238, 48, 28);
    AddChild(hwnd, &st, L"BUTTON", Tr(L"下移"), BS_OWNERDRAW | WS_TABSTOP, IDC_DOWN, 320, 238, 48, 28);
    AddChild(hwnd, &st, L"STATIC", Tr(L"右键图标，选「分组」把它加进来"), 0, IDC_HINT, 12, 280, 270, 24);
    AddChild(hwnd, &st, L"BUTTON", Tr(L"关闭"), BS_OWNERDRAW | WS_TABSTOP, IDCANCEL, 300, 276, 68, 28);
    SetChildFonts(hwnd, st.font);
    RefreshGroupList(hwnd, &st);
    RunModalLoop(hwnd, owner, &st);
    DestroyDlg(hwnd, &st);
}

}  // namespace app
