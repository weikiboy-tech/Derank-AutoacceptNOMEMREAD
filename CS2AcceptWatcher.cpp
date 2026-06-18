#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

namespace {

constexpr wchar_t APP_NAME[] = L"Weiks CS2 Autoaccept";
constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_KEYHOOK_TOGGLE_MACRO = WM_APP + 2;
constexpr UINT WM_KEYHOOK_STOP_MACRO = WM_APP + 3;
constexpr UINT TIMER_ID = 1001;
constexpr int ID_PAUSE_BUTTON = 1;
constexpr int ID_AUTO_CLICK = 2;
constexpr int ID_AUTO_FOCUS = 3;
constexpr int ID_MACRO_TOGGLE = 4;
constexpr int ID_LANGUAGE_TOGGLE = 5;
constexpr int ID_FORCE_ACCEPT_PAUSE = 6;
constexpr int HOTKEY_MACRO_TOGGLE = 10;
constexpr int HOTKEY_MACRO_STOP = 11;
constexpr int TIMER_TICK_MS = 50;
constexpr int DEFAULT_INTERVAL_MS = 700;
constexpr int DEFAULT_MACRO_DELAY_MS = 7900;
constexpr ULONGLONG ACCEPT_PAUSE_MS = 120000;
constexpr double AUDIO_THRESHOLD = 0.015;
constexpr int RECONNECT_CLICK_X = 1500;
constexpr int RECONNECT_CLICK_Y = 66;
constexpr int DERANK_CYCLE_LIMIT = 14;
constexpr int LOBBY_RETURN_CLICK_1_X = 1632;
constexpr int LOBBY_RETURN_CLICK_1_Y = 127;
constexpr int LOBBY_RETURN_CLICK_2_X = 1677;
constexpr int LOBBY_RETURN_CLICK_2_Y = 127;

HWND g_hwnd = nullptr;
HWND g_status = nullptr;
HWND g_log = nullptr;
HWND g_pauseButton = nullptr;
HWND g_autoClick = nullptr;
HWND g_autoFocus = nullptr;
HWND g_macroToggle = nullptr;
HWND g_languageButton = nullptr;
HWND g_forceAcceptPauseButton = nullptr;
HWND g_autoAcceptGroup = nullptr;
HWND g_macroGroup = nullptr;
HWND g_logLabel = nullptr;
HWND g_hotkeyLabel = nullptr;
HWND g_delayLabel = nullptr;
HWND g_acceptPauseLabel = nullptr;
HWND g_intervalLabel = nullptr;
HWND g_consoleHotkeyEdit = nullptr;
HWND g_macroDelayEdit = nullptr;
HWND g_intervalEdit = nullptr;
NOTIFYICONDATAW g_nid{};
HHOOK g_keyboardHook = nullptr;
ULONGLONG g_lastF8Tick = 0;
ULONGLONG g_lastF9Tick = 0;
HWND g_macroTargetHwnd = nullptr;
bool g_running = true;
ULONGLONG g_acceptPauseUntilTick = 0;
ULONGLONG g_lastFocusTick = 0;
ULONGLONG g_nextWatcherTick = 0;
std::wstring g_consoleHotkeyText = L"^";
int g_macroDelayMs = DEFAULT_MACRO_DELAY_MS;

enum class Language {
    German,
    English
};

Language g_language = Language::German;

enum class MacroStep {
    Idle,
    OpenConsole,
    TypeDisconnect,
    PressEnter,
    CloseConsole,
    WaitReconnect,
    ClickReconnect1,
    ClickReconnect2,
    ClickReconnect3,
    LobbyReturnClick1,
    LobbyReturnClick2,
    Delay
};

struct KeySpec {
    bool valid = false;
    bool scanCode = false;
    WORD vk = 0;
    WORD scan = 0;
    bool extended = false;
};

bool g_macroRunning = false;
bool g_macroBusy = false;
MacroStep g_macroStep = MacroStep::Idle;
ULONGLONG g_macroDueTick = 0;
ULONGLONG g_macroNextRunTick = 0;
int g_macroDisconnectCount = 0;
KeySpec g_macroConsoleKey{};

struct Cs2WindowState {
    bool found = false;
    HWND hwnd = nullptr;
    DWORD pid = 0;
    std::wstring title;
    bool foreground = false;
};

struct AcceptCandidate {
    bool found = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    double score = 0;
};

std::wstring NowTime() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[32];
    wsprintfW(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

void SetStatus(const std::wstring& text) {
    if (g_status) SetWindowTextW(g_status, text.c_str());
}

void AddLog(const std::wstring& text) {
    if (!g_log) return;
    std::wstring line = L"[" + NowTime() + L"] " + text + L"\r\n";
    int len = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, len, len);
    SendMessageW(g_log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && g_hwnd) {
        const auto* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        ULONGLONG now = GetTickCount64();

        if (key->vkCode == VK_F8) {
            if (keyDown && now - g_lastF8Tick > 300) {
                g_lastF8Tick = now;
                PostMessageW(g_hwnd, WM_KEYHOOK_TOGGLE_MACRO, 0, reinterpret_cast<LPARAM>(GetForegroundWindow()));
            }
            return 1;
        }

        if (key->vkCode == VK_F9) {
            if (keyDown && now - g_lastF9Tick > 300) {
                g_lastF9Tick = now;
                PostMessageW(g_hwnd, WM_KEYHOOK_STOP_MACRO, 0, 0);
            }
            return 1;
        }
    }

    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

bool InstallKeyboardHook() {
    if (g_keyboardHook) return true;
    g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
    return g_keyboardHook != nullptr;
}

void UninstallKeyboardHook() {
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
}

int ReadInt(HWND edit, int fallback, int minValue, int maxValue) {
    wchar_t buf[32]{};
    GetWindowTextW(edit, buf, 31);
    int value = _wtoi(buf);
    if (value < minValue || value > maxValue) return fallback;
    return value;
}

bool Checked(HWND hwnd) {
    return SendMessageW(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void SetChecked(HWND hwnd, bool checked) {
    if (hwnd) SendMessageW(hwnd, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

const wchar_t* Text(const wchar_t* german, const wchar_t* english) {
    return g_language == Language::German ? german : english;
}

void UpdateLanguage() {
    if (g_languageButton) SetWindowTextW(g_languageButton, g_language == Language::German ? L"GER" : L"ENG");
    if (g_pauseButton) SetWindowTextW(g_pauseButton, g_running ? Text(L"Pause", L"Pause") : Text(L"Start", L"Start"));
    if (g_autoAcceptGroup) SetWindowTextW(g_autoAcceptGroup, Text(L"Autoaccept", L"Auto-accept"));
    if (g_macroGroup) SetWindowTextW(g_macroGroup, Text(L"Derank-Makro", L"Derank macro"));
    if (g_logLabel) SetWindowTextW(g_logLabel, Text(L"Aktivitätslog", L"Activity log"));
    if (g_forceAcceptPauseButton) SetWindowTextW(g_forceAcceptPauseButton, L"FORCE ACCEPT PAUSE");
    if (g_autoClick) SetWindowTextW(g_autoClick, Text(L"ACCEPT automatisch klicken", L"Auto-click ACCEPT"));
    if (g_autoFocus) SetWindowTextW(g_autoFocus, Text(L"CS2 bei Audio automatisch fokussieren", L"Focus CS2 when audio plays"));
    if (g_macroToggle) SetWindowTextW(g_macroToggle, Text(L"Disconnect/Reconnect-Makro", L"Disconnect/reconnect macro"));
    if (g_hotkeyLabel) SetWindowTextW(g_hotkeyLabel, Text(L"Console-Hotkey", L"Console hotkey"));
    if (g_delayLabel) SetWindowTextW(g_delayLabel, Text(L"Makro-Delay (ms)", L"Macro delay (ms)"));
    if (g_acceptPauseLabel) SetWindowTextW(g_acceptPauseLabel, Text(L"Pause nach ACCEPT: 2 Minuten", L"Pause after ACCEPT: 2 minutes"));
    if (g_intervalLabel) SetWindowTextW(g_intervalLabel, Text(L"Scan-Intervall (ms)", L"Scan interval (ms)"));
}

void ToggleLanguage() {
    g_language = g_language == Language::German ? Language::English : Language::German;
    UpdateLanguage();
    SetStatus(Text(L"Sprache auf Deutsch gestellt.", L"Language set to English."));
}

void ForceAcceptPause() {
    g_acceptPauseUntilTick = GetTickCount64() + ACCEPT_PAUSE_MS;
    if (g_macroRunning) {
        g_macroBusy = false;
        g_macroStep = MacroStep::Delay;
        g_macroNextRunTick = g_acceptPauseUntilTick;
        g_macroDueTick = g_acceptPauseUntilTick;
    }
    SetStatus(Text(L"ACCEPT-Pause manuell gestartet: 2 Minuten.", L"Accept pause forced: 2 minutes."));
    AddLog(Text(L"FORCE ACCEPT PAUSE: Überwachung pausiert 2 Minuten.", L"FORCE ACCEPT PAUSE: automation paused for 2 minutes."));
}

std::wstring Lower(std::wstring value) {
    for (auto& c : value) c = static_cast<wchar_t>(std::towlower(c));
    return value;
}

std::wstring Trim(std::wstring value) {
    while (!value.empty() && std::iswspace(value.front())) value.erase(value.begin());
    while (!value.empty() && std::iswspace(value.back())) value.pop_back();
    return value;
}

std::wstring ReadText(HWND edit, const std::wstring& fallback) {
    wchar_t buf[256]{};
    if (!edit || !GetWindowTextW(edit, buf, 255)) return fallback;
    std::wstring value = Trim(buf);
    return value.empty() ? fallback : value;
}

std::wstring AppDirectory() {
    wchar_t path[MAX_PATH * 4]{};
    GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    std::wstring fullPath = path;
    size_t slash = fullPath.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return L".";
    return fullPath.substr(0, slash);
}

std::wstring SettingsPath() {
    return AppDirectory() + L"\\WeiksCS2Autoaccept.ini";
}

void LoadSettings() {
    std::wstring ini = SettingsPath();
    wchar_t hotkey[64]{};
    GetPrivateProfileStringW(L"Macro", L"ConsoleHotkey", L"^", hotkey, 64, ini.c_str());
    g_consoleHotkeyText = Trim(hotkey);
    if (g_consoleHotkeyText.empty() || g_consoleHotkeyText == L"^^") g_consoleHotkeyText = L"^";
    g_macroDelayMs = GetPrivateProfileIntW(L"Macro", L"DelayMs", DEFAULT_MACRO_DELAY_MS, ini.c_str());
    if (g_macroDelayMs < 500 || g_macroDelayMs > 60000) g_macroDelayMs = DEFAULT_MACRO_DELAY_MS;

    wchar_t language[16]{};
    GetPrivateProfileStringW(L"UI", L"Language", L"GER", language, 16, ini.c_str());
    g_language = Lower(Trim(language)) == L"eng" ? Language::English : Language::German;
}

void SaveSettings() {
    if (g_consoleHotkeyEdit) g_consoleHotkeyText = ReadText(g_consoleHotkeyEdit, L"^");
    if (g_consoleHotkeyText == L"^^") g_consoleHotkeyText = L"^";
    if (g_macroDelayEdit) g_macroDelayMs = ReadInt(g_macroDelayEdit, DEFAULT_MACRO_DELAY_MS, 500, 60000);

    std::wstring ini = SettingsPath();
    WritePrivateProfileStringW(L"Macro", L"ConsoleHotkey", g_consoleHotkeyText.c_str(), ini.c_str());
    wchar_t delay[32]{};
    wsprintfW(delay, L"%d", g_macroDelayMs);
    WritePrivateProfileStringW(L"Macro", L"DelayMs", delay, ini.c_str());
    WritePrivateProfileStringW(L"UI", L"Language", g_language == Language::German ? L"GER" : L"ENG", ini.c_str());
}

bool TryParseUInt(const std::wstring& text, int base, WORD& value) {
    wchar_t* end = nullptr;
    unsigned long parsed = wcstoul(text.c_str(), &end, base);
    if (!end || end == text.c_str() || *end != L'\0' || parsed > 0xFF) return false;
    value = static_cast<WORD>(parsed);
    return true;
}

KeySpec ParseConsoleHotkey(const std::wstring& raw) {
    std::wstring value = Lower(Trim(raw));
    KeySpec key;

    if (value == L"^^" || value == L"^" || value == L"{sc029}" || value == L"sc029" || value == L"0x29") {
        key.valid = true;
        key.scanCode = true;
        key.scan = 0x29;
        return key;
    }

    if (value.size() >= 4 && value.front() == L'{' && value.back() == L'}') {
        value = value.substr(1, value.size() - 2);
    }

    if (value.rfind(L"sc", 0) == 0) {
        WORD scan = 0;
        if (TryParseUInt(value.substr(2), 16, scan)) {
            key.valid = true;
            key.scanCode = true;
            key.scan = scan;
            return key;
        }
    }

    if (value.rfind(L"0x", 0) == 0) {
        WORD scan = 0;
        if (TryParseUInt(value.substr(2), 16, scan)) {
            key.valid = true;
            key.scanCode = true;
            key.scan = scan;
            return key;
        }
    }

    if (value.size() >= 2 && value[0] == L'f') {
        int number = _wtoi(value.c_str() + 1);
        if (number >= 1 && number <= 24) {
            key.valid = true;
            key.vk = static_cast<WORD>(VK_F1 + number - 1);
            return key;
        }
    }

    if (value.size() == 1) {
        SHORT mapped = VkKeyScanW(value[0]);
        if (mapped != -1) {
            key.valid = true;
            key.vk = LOBYTE(mapped);
            return key;
        }
    }

    return key;
}

std::wstring FileNameFromPath(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return path;
    return path.substr(slash + 1);
}

std::wstring ProcessImageName(DWORD pid) {
    std::wstring result;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return result;
    wchar_t path[MAX_PATH * 4]{};
    DWORD size = static_cast<DWORD>(std::size(path));
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        result.assign(path, size);
    }
    CloseHandle(process);
    return result;
}

std::wstring WindowTitle(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    std::wstring title(static_cast<size_t>(std::max(len, 0)), L'\0');
    if (len > 0) {
        GetWindowTextW(hwnd, title.data(), len + 1);
    }
    return title;
}

struct EnumData {
    Cs2WindowState best;
    int bestArea = -1;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lparam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return TRUE;

    std::wstring title = WindowTitle(hwnd);
    std::wstring proc = Lower(FileNameFromPath(ProcessImageName(pid)));
    std::wstring lowTitle = Lower(title);

    bool isCs2 = proc == L"cs2.exe" || lowTitle.find(L"counter-strike 2") != std::wstring::npos;
    if (!isCs2) return TRUE;

    RECT r{};
    int area = 0;
    if (GetWindowRect(hwnd, &r)) {
        area = std::max(0L, r.right - r.left) * std::max(0L, r.bottom - r.top);
    }

    auto* data = reinterpret_cast<EnumData*>(lparam);
    if (area > data->bestArea) {
        data->bestArea = area;
        data->best = { true, hwnd, pid, title, GetForegroundWindow() == hwnd };
    }

    return TRUE;
}

Cs2WindowState GetCs2WindowState() {
    EnumData data;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.best;
}

bool ActivateWindow(HWND hwnd) {
    if (!hwnd) return false;

    ShowWindow(hwnd, SW_RESTORE);

    HWND foreground = GetForegroundWindow();
    DWORD currentThread = GetCurrentThreadId();
    DWORD foregroundThread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    DWORD targetThread = GetWindowThreadProcessId(hwnd, nullptr);

    INPUT altDown{};
    altDown.type = INPUT_KEYBOARD;
    altDown.ki.wVk = VK_MENU;
    SendInput(1, &altDown, sizeof(INPUT));

    if (foregroundThread) AttachThreadInput(currentThread, foregroundThread, TRUE);
    if (targetThread) AttachThreadInput(currentThread, targetThread, TRUE);

    bool ok = SetForegroundWindow(hwnd) != FALSE;
    BringWindowToTop(hwnd);
    SetFocus(hwnd);

    if (targetThread) AttachThreadInput(currentThread, targetThread, FALSE);
    if (foregroundThread) AttachThreadInput(currentThread, foregroundThread, FALSE);

    INPUT altUp{};
    altUp.type = INPUT_KEYBOARD;
    altUp.ki.wVk = VK_MENU;
    altUp.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &altUp, sizeof(INPUT));

    return ok || GetForegroundWindow() == hwnd;
}

void ClickAt(int x, int y) {
    SetCursorPos(x, y);

    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void SendKeySpec(const KeySpec& key) {
    if (!key.valid) return;

    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[1].type = INPUT_KEYBOARD;

    if (key.scanCode) {
        inputs[0].ki.wScan = key.scan;
        inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE | (key.extended ? KEYEVENTF_EXTENDEDKEY : 0);
        inputs[1].ki.wScan = key.scan;
        inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | (key.extended ? KEYEVENTF_EXTENDEDKEY : 0);
    } else {
        inputs[0].ki.wVk = key.vk;
        inputs[1].ki.wVk = key.vk;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    }

    SendInput(2, inputs, sizeof(INPUT));
}

void SendVk(WORD vk) {
    KeySpec key;
    key.valid = true;
    key.vk = vk;
    SendKeySpec(key);
}

void SendUnicodeText(const std::wstring& text) {
    for (wchar_t ch : text) {
        INPUT inputs[2]{};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wScan = ch;
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wScan = ch;
        inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
    }
}

AcceptCandidate FindAcceptInPixels(const std::vector<unsigned char>& pixels, int width, int height, int offsetX, int offsetY, int step) {
    const int cw = (width + step - 1) / step;
    const int ch = (height + step - 1) / step;
    std::vector<unsigned char> mask(static_cast<size_t>(cw * ch), 0);
    std::vector<unsigned char> seen(static_cast<size_t>(cw * ch), 0);

    for (int gy = 0; gy < ch; ++gy) {
        int y = std::min(gy * step, height - 1);
        for (int gx = 0; gx < cw; ++gx) {
            int x = std::min(gx * step, width - 1);
            size_t p = static_cast<size_t>((y * width + x) * 4);
            unsigned char b = pixels[p + 0];
            unsigned char g = pixels[p + 1];
            unsigned char r = pixels[p + 2];

            bool acceptGreen =
                g >= 145 &&
                r >= 20 && r <= 95 &&
                b >= 25 && b <= 135 &&
                g > r * 1.65 &&
                g > b * 1.35;

            mask[static_cast<size_t>(gy * cw + gx)] = acceptGreen ? 1 : 0;
        }
    }

    AcceptCandidate best;
    std::vector<int> stack;
    stack.reserve(static_cast<size_t>(cw * ch));

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    for (int start = 0; start < cw * ch; ++start) {
        if (!mask[start] || seen[start]) continue;

        stack.clear();
        stack.push_back(start);
        seen[start] = 1;

        int count = 0;
        int minX = cw, minY = ch, maxX = 0, maxY = 0;

        while (!stack.empty()) {
            int idx = stack.back();
            stack.pop_back();
            int x = idx % cw;
            int y = idx / cw;
            ++count;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);

            for (int n = 0; n < 4; ++n) {
                int nx = x + dx[n];
                int ny = y + dy[n];
                if (nx < 0 || nx >= cw || ny < 0 || ny >= ch) continue;
                int ni = ny * cw + nx;
                if (!mask[ni] || seen[ni]) continue;
                seen[ni] = 1;
                stack.push_back(ni);
            }
        }

        int cellsW = maxX - minX + 1;
        int cellsH = maxY - minY + 1;
        int physW = cellsW * step;
        int physH = cellsH * step;
        double fill = count / static_cast<double>(cellsW * cellsH);

        bool acceptLike =
            physW >= 95 && physW <= 390 &&
            physH >= 38 && physH <= 170 &&
            fill >= 0.42 &&
            count >= 240;

        if (!acceptLike) continue;

        double score = count * fill * std::min(2.0, physW / 120.0) * std::min(2.0, physH / 55.0);
        if (score > best.score) {
            best.found = true;
            best.x = offsetX + ((minX + maxX + 1) * step) / 2;
            best.y = offsetY + ((minY + maxY + 1) * step) / 2;
            best.width = physW;
            best.height = physH;
            best.score = score;
        }
    }

    return best;
}

AcceptCandidate FindAcceptButton(HWND hwnd) {
    RECT wr{};
    if (!hwnd || !GetWindowRect(hwnd, &wr)) return {};

    RECT screen{
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)
    };

    RECT cap{
        std::max(wr.left, screen.left),
        std::max(wr.top, screen.top),
        std::min(wr.right, screen.right),
        std::min(wr.bottom, screen.bottom)
    };

    int width = cap.right - cap.left;
    int height = cap.bottom - cap.top;
    if (width < 80 || height < 80) return {};

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(memDc, bitmap);
    BitBlt(memDc, 0, 0, width, height, screenDc, cap.left, cap.top, SRCCOPY);

    std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 4));
    if (bits) {
        memcpy(pixels.data(), bits, pixels.size());
    }

    SelectObject(memDc, old);
    DeleteObject(bitmap);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    return FindAcceptInPixels(pixels, width, height, cap.left, cap.top, 3);
}

float GetProcessPeak(DWORD pid) {
    if (!pid) return 0.0f;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioSessionManager2* manager = nullptr;
    IAudioSessionEnumerator* sessions = nullptr;
    float best = 0.0f;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr) || !enumerator) goto cleanup;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    if (FAILED(hr) || !device) goto cleanup;

    hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&manager));
    if (FAILED(hr) || !manager) goto cleanup;

    hr = manager->GetSessionEnumerator(&sessions);
    if (FAILED(hr) || !sessions) goto cleanup;

    int count = 0;
    sessions->GetCount(&count);

    for (int i = 0; i < count; ++i) {
        IAudioSessionControl* control = nullptr;
        IAudioSessionControl2* control2 = nullptr;
        IAudioMeterInformation* meter = nullptr;

        if (SUCCEEDED(sessions->GetSession(i, &control)) && control) {
            if (SUCCEEDED(control->QueryInterface(IID_PPV_ARGS(&control2))) && control2) {
                DWORD sessionPid = 0;
                control2->GetProcessId(&sessionPid);
                if (sessionPid == pid && SUCCEEDED(control->QueryInterface(IID_PPV_ARGS(&meter))) && meter) {
                    float peak = 0.0f;
                    if (SUCCEEDED(meter->GetPeakValue(&peak))) {
                        best = std::max(best, peak);
                    }
                }
            }
        }

        if (meter) meter->Release();
        if (control2) control2->Release();
        if (control) control->Release();
    }

cleanup:
    if (sessions) sessions->Release();
    if (manager) manager->Release();
    if (device) device->Release();
    if (enumerator) enumerator->Release();
    return best;
}

void SyncMacroToggle() {
    SetChecked(g_macroToggle, g_macroRunning);
}

void StopMacro(const std::wstring& reason) {
    bool wasRunning = g_macroRunning || g_macroBusy || g_macroStep != MacroStep::Idle;
    g_macroRunning = false;
    g_macroBusy = false;
    g_macroStep = MacroStep::Idle;
    g_macroDueTick = 0;
    g_macroNextRunTick = 0;
    g_macroTargetHwnd = nullptr;
    g_macroDisconnectCount = 0;
    g_lastF8Tick = 0;
    g_lastF9Tick = 0;
    SyncMacroToggle();
    if (wasRunning) AddLog(reason);
    SetStatus(Text(L"Disconnect/Reconnect-Makro gestoppt.", L"Disconnect/reconnect macro stopped."));
}

HWND PickMacroTarget(HWND preferredTarget) {
    if (preferredTarget && IsWindow(preferredTarget) && preferredTarget != g_hwnd) {
        return preferredTarget;
    }

    HWND foreground = GetForegroundWindow();
    if (foreground && IsWindow(foreground) && foreground != g_hwnd) {
        return foreground;
    }

    Cs2WindowState state = GetCs2WindowState();
    return state.found ? state.hwnd : nullptr;
}

bool StartMacro(HWND preferredTarget = nullptr) {
    SaveSettings();
    g_macroConsoleKey = ParseConsoleHotkey(g_consoleHotkeyText);
    if (!g_macroConsoleKey.valid) {
        StopMacro(Text(L"Makro nicht gestartet: ungültiger Console-Hotkey.", L"Macro not started: invalid console hotkey."));
        SetStatus(Text(L"Ungueltiger Console-Hotkey. Nutze ^, SC029, F1 oder eine einzelne Taste.", L"Invalid console hotkey. Use ^, SC029, F1, or one single key."));
        return false;
    }

    g_macroTargetHwnd = PickMacroTarget(preferredTarget);
    if (!g_macroTargetHwnd) {
        StopMacro(Text(L"Makro nicht gestartet: kein Zielfenster gefunden.", L"Macro not started: no target window found."));
        SetStatus(Text(L"Kein Zielfenster gefunden. Starte mit F8, waehrend CS2 aktiv ist.", L"No target window found. Press F8 while CS2 is active."));
        return false;
    }

    g_macroRunning = true;
    g_macroBusy = false;
    g_macroStep = MacroStep::OpenConsole;
    g_macroDueTick = GetTickCount64();
    g_macroNextRunTick = 0;
    g_macroDisconnectCount = 0;
    SyncMacroToggle();

    std::wstringstream ss;
    ss << Text(L"Disconnect/Reconnect-Makro gestartet. Delay=", L"Disconnect/reconnect macro started. Delay=") << g_macroDelayMs << L" ms, Hotkey=" << g_consoleHotkeyText;
    AddLog(ss.str());
    SetStatus(Text(L"Disconnect/Reconnect-Makro gestartet.", L"Disconnect/reconnect macro started."));
    return true;
}

void ToggleMacro(HWND preferredTarget = nullptr) {
    if (g_macroRunning) {
        StopMacro(Text(L"Disconnect/Reconnect-Makro gestoppt.", L"Disconnect/reconnect macro stopped."));
    } else {
        StartMacro(preferredTarget);
    }
}

void ScheduleMacroStep(MacroStep step, ULONGLONG delayMs) {
    g_macroStep = step;
    g_macroDueTick = GetTickCount64() + delayMs;
}

void ProcessMacro(ULONGLONG now) {
    if (!g_macroRunning || now < g_macroDueTick) return;

    if (g_macroStep == MacroStep::Delay) {
        if (now < g_macroNextRunTick) return;
        g_macroStep = MacroStep::OpenConsole;
    }

    if (g_macroStep == MacroStep::OpenConsole) {
        if (!g_macroTargetHwnd || !IsWindow(g_macroTargetHwnd)) {
            SetStatus(Text(L"Makro wartet auf Zielfenster.", L"Macro waiting for target window."));
            g_macroDueTick = now + 1000;
            return;
        }

        g_macroBusy = true;
        if (GetForegroundWindow() != g_macroTargetHwnd) {
            ActivateWindow(g_macroTargetHwnd);
            Sleep(80);
        }
        SendKeySpec(g_macroConsoleKey);
        ScheduleMacroStep(MacroStep::TypeDisconnect, 180);
        return;
    }

    if (g_macroStep == MacroStep::TypeDisconnect) {
        if (g_macroTargetHwnd && IsWindow(g_macroTargetHwnd)) {
            ActivateWindow(g_macroTargetHwnd);
            Sleep(60);
        }
        SendUnicodeText(L"disconnect");
        g_macroDisconnectCount++;
        ScheduleMacroStep(MacroStep::PressEnter, 60);
        return;
    }

    if (g_macroStep == MacroStep::PressEnter) {
        SendVk(VK_RETURN);
        ScheduleMacroStep(MacroStep::CloseConsole, 40);
        return;
    }

    if (g_macroStep == MacroStep::CloseConsole) {
        SendKeySpec(g_macroConsoleKey);
        if (g_macroDisconnectCount >= DERANK_CYCLE_LIMIT) {
            ScheduleMacroStep(MacroStep::LobbyReturnClick1, 500);
            return;
        }
        ScheduleMacroStep(MacroStep::WaitReconnect, 500);
        return;
    }

    if (g_macroStep == MacroStep::LobbyReturnClick1) {
        if (g_macroTargetHwnd && IsWindow(g_macroTargetHwnd)) {
            ActivateWindow(g_macroTargetHwnd);
            Sleep(80);
        }
        ClickAt(LOBBY_RETURN_CLICK_1_X, LOBBY_RETURN_CLICK_1_Y);
        ScheduleMacroStep(MacroStep::LobbyReturnClick2, 250);
        return;
    }

    if (g_macroStep == MacroStep::LobbyReturnClick2) {
        ClickAt(LOBBY_RETURN_CLICK_2_X, LOBBY_RETURN_CLICK_2_Y);
        StopMacro(Text(L"Derank-Makro nach 14 Disconnects beendet. Lobby-Return geklickt.", L"Derank macro stopped after 14 disconnects. Lobby return clicked."));
        return;
    }

    if (g_macroStep == MacroStep::WaitReconnect) {
        if (g_macroTargetHwnd && IsWindow(g_macroTargetHwnd)) {
            ActivateWindow(g_macroTargetHwnd);
            Sleep(80);
        }
        ScheduleMacroStep(MacroStep::ClickReconnect1, 40);
        return;
    }

    if (g_macroStep == MacroStep::ClickReconnect1) {
        ClickAt(RECONNECT_CLICK_X, RECONNECT_CLICK_Y);
        ScheduleMacroStep(MacroStep::ClickReconnect2, 250);
        return;
    }

    if (g_macroStep == MacroStep::ClickReconnect2) {
        ClickAt(RECONNECT_CLICK_X, RECONNECT_CLICK_Y);
        ScheduleMacroStep(MacroStep::ClickReconnect3, 250);
        return;
    }

    if (g_macroStep == MacroStep::ClickReconnect3) {
        ClickAt(RECONNECT_CLICK_X, RECONNECT_CLICK_Y);
        g_macroBusy = false;
        g_macroDelayMs = ReadInt(g_macroDelayEdit, DEFAULT_MACRO_DELAY_MS, 500, 60000);
        g_macroNextRunTick = GetTickCount64() + static_cast<ULONGLONG>(g_macroDelayMs);
        g_macroStep = MacroStep::Delay;
        g_macroDueTick = g_macroNextRunTick;

        std::wstringstream ss;
        ss << Text(L"Makro-Cycle ", L"Macro cycle ") << g_macroDisconnectCount << L"/" << DERANK_CYCLE_LIMIT << Text(L" ausgeführt. Nächster Lauf in ", L" completed. Next run in ") << g_macroDelayMs << L" ms.";
        AddLog(ss.str());
        SetStatus(ss.str());
        return;
    }
}

void Tick() {
    if (!g_running) return;

    ULONGLONG now = GetTickCount64();
    if (g_acceptPauseUntilTick > now) {
        ULONGLONG remainingMs = g_acceptPauseUntilTick - now;
        ULONGLONG remainingSec = (remainingMs + 999) / 1000;
        std::wstringstream ss;
        ss << Text(L"Nach ACCEPT pausiert. Weiter in ", L"Accept pause active. Resumes in ") << remainingSec << Text(L" Sekunden.", L" seconds.");
        SetStatus(ss.str());
        return;
    }

    ProcessMacro(now);

    if (now < g_nextWatcherTick) return;
    g_nextWatcherTick = now + static_cast<ULONGLONG>(ReadInt(g_intervalEdit, DEFAULT_INTERVAL_MS, 250, 3000));

    Cs2WindowState state = GetCs2WindowState();
    if (!state.found) {
        SetStatus(Text(L"CS2-Fenster nicht gefunden.", L"CS2 window not found."));
        return;
    }

    float peak = 0.0f;

    if (Checked(g_autoClick)) {
        AcceptCandidate c = FindAcceptButton(state.hwnd);
        if (c.found) {
            HWND previousForeground = GetForegroundWindow();
            bool restorePreviousForeground = previousForeground && previousForeground != state.hwnd;
            g_acceptPauseUntilTick = GetTickCount64() + ACCEPT_PAUSE_MS;
            if (g_macroRunning) {
                g_macroBusy = false;
                g_macroStep = MacroStep::Delay;
                g_macroNextRunTick = g_acceptPauseUntilTick;
                g_macroDueTick = g_acceptPauseUntilTick;
            }

            if (GetForegroundWindow() != state.hwnd) {
                ActivateWindow(state.hwnd);
                Sleep(120);
            }
            ClickAt(c.x, c.y);
            if (restorePreviousForeground && IsWindow(previousForeground)) {
                Sleep(150);
                ActivateWindow(previousForeground);
            }

            std::wstringstream status;
            status << Text(L"ACCEPT geklickt bei X=", L"ACCEPT clicked at X=") << c.x << L", Y=" << c.y << Text(L". Pausiere 2 Minuten.", L". Pausing for 2 minutes.");
            SetStatus(status.str());

            std::wstringstream log;
            log << Text(L"ACCEPT geklickt. Pos=", L"ACCEPT clicked. Pos=") << c.x << L"," << c.y << Text(L" Größe=", L" Size=") << c.width << L"x" << c.height << Text(L". Überwachung pausiert 2 Minuten.", L". Automation paused for 2 minutes.");
            AddLog(log.str());
            return;
        }
    }

    if (g_acceptPauseUntilTick > GetTickCount64()) {
        SetStatus(Text(L"Nach ACCEPT pausiert. Audio/Fokus bleibt aus.", L"Accept pause active. Audio/focus disabled."));
        return;
    }

    if (Checked(g_autoFocus) && !state.foreground) {
        peak = GetProcessPeak(state.pid);
        if (peak >= AUDIO_THRESHOLD && now - g_lastFocusTick > 3000) {
            ActivateWindow(state.hwnd);
            g_lastFocusTick = now;
            std::wstringstream ss;
            ss << Text(L"CS2 fokussiert, weil Audio erkannt wurde. Peak=", L"CS2 focused because audio was detected. Peak=") << peak;
            AddLog(ss.str());
        }
    }

    std::wstringstream ss;
    ss << Text(L"CS2 gefunden. Vordergrund=", L"CS2 found. Foreground=") << (GetForegroundWindow() == state.hwnd ? Text(L"ja", L"yes") : Text(L"nein", L"no"));
    if (Checked(g_autoFocus)) ss << L", AudioPeak=" << peak;
    SetStatus(ss.str());
}

void AddControls(HWND hwnd) {
    HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    HWND title = CreateWindowW(L"STATIC", APP_NAME, WS_CHILD | WS_VISIBLE, 18, 14, 280, 24, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_languageButton = CreateWindowW(L"BUTTON", L"GER", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 432, 12, 58, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LANGUAGE_TOGGLE)), nullptr, nullptr);
    SendMessageW(g_languageButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_status = CreateWindowW(L"STATIC", L"Initialisiere...", WS_CHILD | WS_VISIBLE, 18, 46, 472, 28, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_autoAcceptGroup = CreateWindowW(L"BUTTON", L"Autoaccept", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 18, 82, 472, 154, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_autoAcceptGroup, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_pauseButton = CreateWindowW(L"BUTTON", L"Pause", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 34, 112, 92, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PAUSE_BUTTON)), nullptr, nullptr);
    SendMessageW(g_pauseButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_autoClick = CreateWindowW(L"BUTTON", L"ACCEPT automatisch klicken", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 150, 112, 260, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_AUTO_CLICK)), nullptr, nullptr);
    SendMessageW(g_autoClick, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(g_autoClick, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_autoFocus = CreateWindowW(L"BUTTON", L"CS2 bei Audio automatisch fokussieren", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 150, 142, 300, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_AUTO_FOCUS)), nullptr, nullptr);
    SendMessageW(g_autoFocus, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(g_autoFocus, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_forceAcceptPauseButton = CreateWindowW(L"BUTTON", L"FORCE ACCEPT PAUSE", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 34, 166, 168, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FORCE_ACCEPT_PAUSE)), nullptr, nullptr);
    SendMessageW(g_forceAcceptPauseButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_acceptPauseLabel = CreateWindowW(L"STATIC", L"Pause nach ACCEPT: 2 Minuten", WS_CHILD | WS_VISIBLE, 220, 171, 240, 22, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_acceptPauseLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_intervalLabel = CreateWindowW(L"STATIC", L"Scan-Intervall (ms)", WS_CHILD | WS_VISIBLE, 34, 204, 122, 22, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_intervalLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    g_intervalEdit = CreateWindowW(L"EDIT", L"700", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 160, 200, 58, 24, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_intervalEdit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_macroGroup = CreateWindowW(L"BUTTON", L"Derank-Makro", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 18, 258, 472, 112, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_macroGroup, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_macroToggle = CreateWindowW(L"BUTTON", L"Disconnect/Reconnect-Makro", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 34, 288, 240, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_MACRO_TOGGLE)), nullptr, nullptr);
    SendMessageW(g_macroToggle, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_hotkeyLabel = CreateWindowW(L"STATIC", L"Console-Hotkey", WS_CHILD | WS_VISIBLE, 34, 330, 105, 22, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_hotkeyLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    g_consoleHotkeyEdit = CreateWindowW(L"EDIT", g_consoleHotkeyText.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER, 144, 326, 60, 24, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_consoleHotkeyEdit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_delayLabel = CreateWindowW(L"STATIC", L"Makro-Delay (ms)", WS_CHILD | WS_VISIBLE, 244, 330, 118, 22, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_delayLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    wchar_t delayText[32]{};
    wsprintfW(delayText, L"%d", g_macroDelayMs);
    g_macroDelayEdit = CreateWindowW(L"EDIT", delayText, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 366, 326, 72, 24, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_macroDelayEdit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_logLabel = CreateWindowW(L"STATIC", L"Aktivitätslog", WS_CHILD | WS_VISIBLE, 18, 390, 160, 22, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_logLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    g_log = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, 18, 416, 472, 110, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(g_log, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    UpdateLanguage();
}

void AddTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(g_nid.szTip, APP_NAME, ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void DrawDashedSeparator(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    HPEN pen = CreatePen(PS_DOT, 1, RGB(135, 135, 135));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, 34, 246, nullptr);
    LineTo(hdc, 474, 246);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        LoadSettings();
        AddControls(hwnd);
        AddTrayIcon(hwnd);
        if (InstallKeyboardHook()) {
            AddLog(Text(L"Globaler Keyboard-Hook aktiv: F8 Start/Stop, F9 Sofortstopp.", L"Global keyboard hook active: F8 start/stop, F9 instant stop."));
        } else {
            AddLog(Text(L"Keyboard-Hook konnte nicht gestartet werden; versuche RegisterHotKey-Fallback.", L"Keyboard hook could not start; trying RegisterHotKey fallback."));
            if (!RegisterHotKey(hwnd, HOTKEY_MACRO_TOGGLE, 0, VK_F8)) {
                AddLog(Text(L"RegisterHotKey F8 nicht verfügbar.", L"RegisterHotKey F8 unavailable."));
            }
            if (!RegisterHotKey(hwnd, HOTKEY_MACRO_STOP, 0, VK_F9)) {
                AddLog(Text(L"RegisterHotKey F9 nicht verfügbar.", L"RegisterHotKey F9 unavailable."));
            }
        }
        SetTimer(hwnd, TIMER_ID, TIMER_TICK_MS, nullptr);
        AddLog(Text(L"Bereit. Minimiere das Fenster, wenn es im Hintergrund laufen soll.", L"Ready. Minimize the window to keep it running in the background."));
        return 0;

    case WM_PAINT:
        DrawDashedSeparator(hwnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wparam) == ID_PAUSE_BUTTON) {
            g_running = !g_running;
            UpdateLanguage();
            SetStatus(g_running ? Text(L"Watcher läuft.", L"Watcher running.") : Text(L"Pausiert.", L"Paused."));
            AddLog(g_running ? Text(L"Watcher läuft.", L"Watcher running.") : Text(L"Watcher pausiert.", L"Watcher paused."));
        } else if (LOWORD(wparam) == ID_MACRO_TOGGLE) {
            if (Checked(g_macroToggle) && !g_macroRunning) {
                StartMacro();
            } else if (!Checked(g_macroToggle) && g_macroRunning) {
                StopMacro(Text(L"Disconnect/Reconnect-Makro gestoppt.", L"Disconnect/reconnect macro stopped."));
            }
        } else if (LOWORD(wparam) == ID_LANGUAGE_TOGGLE) {
            ToggleLanguage();
            SaveSettings();
        } else if (LOWORD(wparam) == ID_FORCE_ACCEPT_PAUSE) {
            ForceAcceptPause();
        }
        return 0;

    case WM_HOTKEY:
        if (wparam == HOTKEY_MACRO_TOGGLE) {
            ToggleMacro(GetForegroundWindow());
        } else if (wparam == HOTKEY_MACRO_STOP) {
            StopMacro(Text(L"Disconnect/Reconnect-Makro sofort gestoppt.", L"Disconnect/reconnect macro stopped instantly."));
        }
        return 0;

    case WM_KEYHOOK_TOGGLE_MACRO:
        ToggleMacro(reinterpret_cast<HWND>(lparam));
        return 0;

    case WM_KEYHOOK_STOP_MACRO:
        StopMacro(Text(L"Disconnect/Reconnect-Makro sofort gestoppt.", L"Disconnect/reconnect macro stopped instantly."));
        return 0;

    case WM_TIMER:
        if (wparam == TIMER_ID) {
            Tick();
        }
        return 0;

    case WM_SIZE:
        if (wparam == SIZE_MINIMIZED) {
            ShowWindow(hwnd, SW_HIDE);
            g_nid.uFlags = NIF_INFO;
            lstrcpynW(g_nid.szInfoTitle, APP_NAME, ARRAYSIZE(g_nid.szInfoTitle));
            lstrcpynW(g_nid.szInfo, Text(L"Läuft weiter im Hintergrund. Doppelklick aufs Tray-Icon öffnet das Fenster.", L"Still running in the background. Double-click the tray icon to open the window."), ARRAYSIZE(g_nid.szInfo));
            g_nid.dwInfoFlags = NIIF_INFO;
            Shell_NotifyIconW(NIM_MODIFY, &g_nid);
        }
        return 0;

    case WM_TRAYICON:
        if (lparam == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_SHOW);
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        SaveSettings();
        UninstallKeyboardHook();
        UnregisterHotKey(hwnd, HOTKEY_MACRO_TOGGLE);
        UnregisterHotKey(hwnd, HOTKEY_MACRO_STOP);
        KillTimer(hwnd, TIMER_ID);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCmd) {
    SetProcessDPIAware();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"WeiksCS2AutoacceptWindow";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        APP_NAME,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        526,
        590,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd) {
        CoUninitialize();
        return 1;
    }

    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
