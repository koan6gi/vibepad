#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <objbase.h> 
#include <commctrl.h> 
#include <commdlg.h> 
#include <shellapi.h> 
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "Config.h"
#include "AudioEngine.h"
#include "Utils.h"

ConfigManager g_config;
AudioEngine   g_engine;

HWND hMainWnd = NULL;
HWND hList = NULL;
HWND hComboMic = NULL;
HWND hComboCable = NULL;
HWND hComboMonitor = NULL;
HWND hBtnSetHotkey = NULL;

bool g_isRecordingHotkey = false;
int  g_recordingIndex = -1;

HFONT hFontNormal = NULL;

enum {
    ID_LIST_SOUNDS = 1001,
    ID_BTN_ADD,
    ID_BTN_REMOVE,
    ID_BTN_PLAY,
    ID_BTN_STOP,
    ID_BTN_STOP_ALL,
    ID_BTN_SET_HOTKEY,
    ID_SLIDER_MIC,
    ID_SLIDER_SOUND,
    ID_COMBO_MIC,
    ID_COMBO_CABLE,
    ID_COMBO_MONITOR,
    ID_TRAY_ICON = 2000,
    ID_TRAY_EXIT,
    ID_TRAY_OPEN
};

const UINT WM_TRAY = WM_USER + 1;
const int HOTKEY_ID_BASE = 5000;
const int HOTKEY_ID_PANIC = 4999;

// -----------------------------------------------------------------------------
// HELPERS
// -----------------------------------------------------------------------------

void CreateFonts() {
    hFontNormal = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void SetFont(HWND hWnd) {
    SendMessage(hWnd, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
}

bool AreDevicesConfigured() {
    return !g_config.GetInputDeviceId().empty() &&
        !g_config.GetOutputDeviceId().empty() &&
        !g_config.GetMonitorDeviceId().empty();
}

void UnregisterAllHotkeys(HWND hWnd) {
    if (hWnd == NULL) return;
    UnregisterHotKey(hWnd, HOTKEY_ID_PANIC);
    for (int i = 0; i < 1000; ++i) UnregisterHotKey(hWnd, HOTKEY_ID_BASE + i);
}

void RegisterConfigHotkeys(HWND hWndOverride = NULL) {
    HWND targetWnd = (hWndOverride != NULL) ? hWndOverride : hMainWnd;
    if (targetWnd == NULL) return;

    UnregisterAllHotkeys(targetWnd);
    RegisterHotKey(targetWnd, HOTKEY_ID_PANIC, MOD_ALT | 0x4000, VK_BACK);

    const auto& sounds = g_config.GetSounds();
    for (int i = 0; i < (int)sounds.size(); ++i) {
        if (sounds[i].hotkey > 0) {
            RegisterHotKey(targetWnd, HOTKEY_ID_BASE + i, sounds[i].modifiers | 0x4000, sounds[i].hotkey);
        }
    }
}

std::wstring GetKeyString(int vk, int mods) {
    if (vk == 0) return L"-";
    std::wstringstream ss;
    if (mods & MOD_CONTROL) ss << L"Ctrl + ";
    if (mods & MOD_SHIFT)   ss << L"Shift + ";
    if (mods & MOD_ALT)     ss << L"Alt + ";

    if (vk >= '0' && vk <= '9') ss << (char)vk;
    else if (vk >= 'A' && vk <= 'Z') ss << (char)vk;
    else if (vk >= VK_F1 && vk <= VK_F12) ss << L"F" << (vk - VK_F1 + 1);
    else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) ss << L"Num " << (vk - VK_NUMPAD0);
    else {
        switch (vk) {
        case VK_MULTIPLY: ss << L"Num *"; break;
        case VK_ADD:      ss << L"Num +"; break;
        case VK_SUBTRACT: ss << L"Num -"; break;
        case VK_DECIMAL:  ss << L"Num ."; break;
        case VK_DIVIDE:   ss << L"Num /"; break;
        case VK_UP: ss << L"Up"; break;
        case VK_DOWN: ss << L"Down"; break;
        case VK_LEFT: ss << L"Left"; break;
        case VK_RIGHT: ss << L"Right"; break;
        case VK_SPACE: ss << L"Space"; break;
        case VK_BACK: ss << L"Backsp"; break;
        case VK_RETURN: ss << L"Enter"; break;
        default: ss << L"Key " << vk; break;
        }
    }
    return ss.str();
}

void RefreshSoundList() {
    ListView_DeleteAllItems(hList);
    const auto& sounds = g_config.GetSounds();
    LVITEMW li = { 0 };
    li.mask = LVIF_TEXT | LVIF_PARAM;
    for (int i = 0; i < (int)sounds.size(); ++i) {
        li.iItem = i; li.iSubItem = 0; li.lParam = i;
        li.pszText = (LPWSTR)sounds[i].name.c_str();
        ListView_InsertItem(hList, &li);
        std::wstring hkStr = GetKeyString(sounds[i].hotkey, sounds[i].modifiers);
        ListView_SetItemText(hList, i, 1, (LPWSTR)hkStr.c_str());
    }
}

void PopulateDeviceCombos() {
    g_engine.RefreshDeviceList();
    auto inputs = g_engine.GetInputDevices();
    auto outputs = g_engine.GetOutputDevices();

    SendMessage(hComboMic, CB_RESETCONTENT, 0, 0);
    SendMessage(hComboMic, CB_ADDSTRING, 0, (LPARAM)L"Select Microphone...");
    int selMic = 0;
    for (size_t i = 0; i < inputs.size(); ++i) {
        std::wstring wName = Utils::Utf8ToWide(inputs[i].name);
        SendMessage(hComboMic, CB_ADDSTRING, 0, (LPARAM)wName.c_str());
        if (inputs[i].name == g_config.GetInputDeviceId()) selMic = (int)i + 1;
    }
    SendMessage(hComboMic, CB_SETCURSEL, selMic, 0);

    SendMessage(hComboCable, CB_RESETCONTENT, 0, 0);
    SendMessage(hComboCable, CB_ADDSTRING, 0, (LPARAM)L"Select Virtual Cable...");
    int selCable = 0;
    for (size_t i = 0; i < outputs.size(); ++i) {
        std::wstring wName = Utils::Utf8ToWide(outputs[i].name);
        SendMessage(hComboCable, CB_ADDSTRING, 0, (LPARAM)wName.c_str());
        if (outputs[i].name == g_config.GetOutputDeviceId()) selCable = (int)i + 1;
    }
    SendMessage(hComboCable, CB_SETCURSEL, selCable, 0);

    SendMessage(hComboMonitor, CB_RESETCONTENT, 0, 0);
    SendMessage(hComboMonitor, CB_ADDSTRING, 0, (LPARAM)L"Select Headphones...");
    int selMon = 0;
    for (size_t i = 0; i < outputs.size(); ++i) {
        std::wstring wName = Utils::Utf8ToWide(outputs[i].name);
        SendMessage(hComboMonitor, CB_ADDSTRING, 0, (LPARAM)wName.c_str());
        if (outputs[i].name == g_config.GetMonitorDeviceId()) selMon = (int)i + 1;
    }
    SendMessage(hComboMonitor, CB_SETCURSEL, selMon, 0);
}

void ApplyDeviceSelection() {
    auto GetComboText = [](HWND hCombo) -> std::string {
        int idx = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
        if (idx <= 0) return "";
        int len = (int)SendMessage(hCombo, CB_GETLBTEXTLEN, idx, 0);
        if (len == CB_ERR) return "";
        std::vector<wchar_t> buf(len + 1);
        SendMessage(hCombo, CB_GETLBTEXT, idx, (LPARAM)buf.data());
        return Utils::WideToUtf8(buf.data());
        };

    std::string mic = GetComboText(hComboMic);
    std::string cable = GetComboText(hComboCable);
    std::string mon = GetComboText(hComboMonitor);

    g_config.SetInputDeviceId(mic);
    g_config.SetOutputDeviceId(cable);
    g_config.SetMonitorDeviceId(mon);

    g_engine.Init(mic, cable, mon);
}

void AddSoundDialog() {
    wchar_t szFile[260] = { 0 };
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hMainWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Audio Files (*.mp3;*.wav)\0*.mp3;*.wav\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        std::wstring fullPath(ofn.lpstrFile);
        std::wstring name = std::filesystem::path(fullPath).stem().wstring();
        if (g_config.AddSound(fullPath, name)) {
            RefreshSoundList();
            RegisterConfigHotkeys();
        }
    }
}

void PlaySelectedSound() {
    if (!AreDevicesConfigured()) {
        MessageBoxW(hMainWnd, L"Please select all audio devices (Input, Output A, Output B) to enable playback.", L"Configuration Required", MB_ICONWARNING);
        return;
    }

    int iPos = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (iPos != -1) {
        LVITEM li = { 0 }; li.iItem = iPos; li.mask = LVIF_PARAM;
        ListView_GetItem(hList, &li);
        const auto& sounds = g_config.GetSounds();
        if ((int)li.lParam < (int)sounds.size())
            g_engine.PlaySoundFile(sounds[li.lParam].GetFullPath());
    }
}

void RemoveSelectedSound() {
    int iPos = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (iPos != -1) {
        LVITEM li = { 0 }; li.iItem = iPos; li.mask = LVIF_PARAM;
        ListView_GetItem(hList, &li);

        const auto& sounds = g_config.GetSounds();
        if ((int)li.lParam < (int)sounds.size()) {
            std::wstring path = sounds[li.lParam].GetFullPath();

            g_engine.FreeSound(path);
        }

        g_config.RemoveSound((int)li.lParam);
        RefreshSoundList();
        RegisterConfigHotkeys();
    }
}

void ToggleHotkeyRecording() {
    if (g_isRecordingHotkey) {
        g_isRecordingHotkey = false;
        g_recordingIndex = -1;
        SetWindowTextW(hBtnSetHotkey, L"⌨ Set Hotkey");
        EnableWindow(hList, TRUE);
        RegisterConfigHotkeys();
        return;
    }

    int iPos = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (iPos == -1) {
        MessageBoxW(hMainWnd, L"Please select a sound first.", L"Info", MB_ICONINFORMATION);
        return;
    }

    LVITEM li = { 0 }; li.iItem = iPos; li.mask = LVIF_PARAM;
    ListView_GetItem(hList, &li);
    g_recordingIndex = (int)li.lParam;

    UnregisterAllHotkeys(hMainWnd);
    g_isRecordingHotkey = true;
    SetWindowTextW(hBtnSetHotkey, L"Press key...");
    SetFocus(hMainWnd);
    EnableWindow(hList, FALSE);
}

void SetupTrayIcon(HWND hWnd, bool add) {
    NOTIFYICONDATA nid = { 0 };
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;

    HICON hIcon = (HICON)LoadImageW(NULL, L"resources\\app.ico", IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);

    if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);

    nid.hIcon = hIcon;
    wcscpy_s(nid.szTip, L"Vibepad");

    if (add) Shell_NotifyIcon(NIM_ADD, &nid);
    else Shell_NotifyIcon(NIM_DELETE, &nid);
}

// -----------------------------------------------------------------------------
// WINDOW PROCEDURE
// -----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

    if (g_isRecordingHotkey) {
        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
            int vk = (int)wParam;
            if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_CAPITAL)
                return DefWindowProc(hWnd, message, wParam, lParam);

            int mods = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
            if (GetKeyState(VK_SHIFT) & 0x8000)   mods |= MOD_SHIFT;
            if (GetKeyState(VK_MENU) & 0x8000)    mods |= MOD_ALT;

            if (vk == VK_ESCAPE) { ToggleHotkeyRecording(); return 0; }

            const auto& sounds = g_config.GetSounds();
            for (int i = 0; i < (int)sounds.size(); ++i) {
                if (i == g_recordingIndex) continue;
                if (sounds[i].hotkey == vk && sounds[i].modifiers == mods) {
                    std::wstring msg = L"Hotkey already used by:\n\"" + sounds[i].name + L"\"";
                    MessageBoxW(hWnd, msg.c_str(), L"Duplicate", MB_ICONWARNING);
                    g_isRecordingHotkey = false;
                    g_recordingIndex = -1;
                    SetWindowTextW(hBtnSetHotkey, L"⌨ Set Hotkey");
                    EnableWindow(hList, TRUE);
                    RegisterConfigHotkeys(hWnd);
                    return 0;
                }
            }

            g_config.SetSoundHotkey(g_recordingIndex, vk, mods);
            g_isRecordingHotkey = false;
            g_recordingIndex = -1;
            SetWindowTextW(hBtnSetHotkey, L"⌨ Set Hotkey");
            EnableWindow(hList, TRUE);
            RefreshSoundList();
            RegisterConfigHotkeys(hWnd);
            return 0;
        }
    }

    switch (message) {
    case WM_CREATE:
    {
        CreateFonts();

        hList = CreateWindowW(WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_VSCROLL,
            15, 15, 420, 190, hWnd, (HMENU)ID_LIST_SOUNDS, NULL, NULL);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        SetFont(hList);

        LVCOLUMNW lvc;
        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

        lvc.iSubItem = 0; lvc.pszText = (LPWSTR)L"Sound Name"; lvc.cx = 295; lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(hList, 0, &lvc);

        lvc.iSubItem = 1; lvc.pszText = (LPWSTR)L"Hotkey"; lvc.cx = 100;
        ListView_InsertColumn(hList, 1, &lvc);

        HWND btn;
        btn = CreateWindowW(L"BUTTON", L"▶ Play", WS_CHILD | WS_VISIBLE, 450, 15, 135, 35, hWnd, (HMENU)ID_BTN_PLAY, NULL, NULL); SetFont(btn);
        btn = CreateWindowW(L"BUTTON", L"⏹ Stop (Alt+Bksp)", WS_CHILD | WS_VISIBLE, 450, 60, 135, 35, hWnd, (HMENU)ID_BTN_STOP_ALL, NULL, NULL); SetFont(btn);
        hBtnSetHotkey = CreateWindowW(L"BUTTON", L"⌨ Set Hotkey", WS_CHILD | WS_VISIBLE, 450, 105, 135, 35, hWnd, (HMENU)ID_BTN_SET_HOTKEY, NULL, NULL); SetFont(hBtnSetHotkey);

        btn = CreateWindowW(L"BUTTON", L"➕ Add Sound", WS_CHILD | WS_VISIBLE, 15, 220, 120, 30, hWnd, (HMENU)ID_BTN_ADD, NULL, NULL); SetFont(btn);
        btn = CreateWindowW(L"BUTTON", L"➖ Remove", WS_CHILD | WS_VISIBLE, 145, 220, 100, 30, hWnd, (HMENU)ID_BTN_REMOVE, NULL, NULL); SetFont(btn);

        HWND grpVol = CreateWindowW(L"BUTTON", L"Volume Mixer", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 15, 265, 560, 80, hWnd, NULL, NULL, NULL); SetFont(grpVol);

        HWND lbl = CreateWindowW(L"STATIC", L"🎤 Mic:", WS_CHILD | WS_VISIBLE, 30, 295, 60, 20, hWnd, NULL, NULL, NULL); SetFont(lbl);
        HWND hMicSlider = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 90, 295, 180, 30, hWnd, (HMENU)ID_SLIDER_MIC, NULL, NULL);
        SendMessage(hMicSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
        SendMessage(hMicSlider, TBM_SETPOS, TRUE, (int)(g_config.GetMicVolume() * 100.0f));
        g_engine.SetMicVolume(g_config.GetMicVolume());

        lbl = CreateWindowW(L"STATIC", L"🔊 Sounds:", WS_CHILD | WS_VISIBLE, 290, 295, 70, 20, hWnd, NULL, NULL, NULL); SetFont(lbl);
        HWND hSndSlider = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 360, 295, 180, 30, hWnd, (HMENU)ID_SLIDER_SOUND, NULL, NULL);
        SendMessage(hSndSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
        SendMessage(hSndSlider, TBM_SETPOS, TRUE, (int)(g_config.GetSoundVolume() * 100.0f));
        g_engine.SetSoundVolume(g_config.GetSoundVolume());

        HWND grpDev = CreateWindowW(L"BUTTON", L"Audio Devices Configuration", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 15, 355, 560, 160, hWnd, NULL, NULL, NULL); SetFont(grpDev);

        lbl = CreateWindowW(L"STATIC", L"Input (Microphone):", WS_CHILD | WS_VISIBLE, 30, 385, 150, 20, hWnd, NULL, NULL, NULL); SetFont(lbl);
        hComboMic = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 30, 405, 240, 200, hWnd, (HMENU)ID_COMBO_MIC, NULL, NULL); SetFont(hComboMic);

        lbl = CreateWindowW(L"STATIC", L"Output A (Virtual Cable):", WS_CHILD | WS_VISIBLE, 300, 385, 200, 20, hWnd, NULL, NULL, NULL); SetFont(lbl);
        hComboCable = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 300, 405, 240, 200, hWnd, (HMENU)ID_COMBO_CABLE, NULL, NULL); SetFont(hComboCable);

        lbl = CreateWindowW(L"STATIC", L"Output B (Headphones/Monitor):", WS_CHILD | WS_VISIBLE, 30, 440, 250, 20, hWnd, NULL, NULL, NULL); SetFont(lbl);
        hComboMonitor = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 30, 460, 510, 200, hWnd, (HMENU)ID_COMBO_MONITOR, NULL, NULL); SetFont(hComboMonitor);

        RefreshSoundList();
        PopulateDeviceCombos();
        ApplyDeviceSelection();
        SetupTrayIcon(hWnd, true);
        RegisterConfigHotkeys(hWnd);
    }
    break;

    case WM_HOTKEY:
    {
        int id = (int)wParam;

        if (id == HOTKEY_ID_PANIC) {
            g_engine.StopAllSounds();
        }
        else {
            if (!AreDevicesConfigured()) {
                MessageBoxW(hMainWnd, L"Please select all audio devices (Input, Output A, Output B) to enable playback.", L"Configuration Required", MB_ICONWARNING | MB_TOPMOST);
                break;
            }

            int soundIndex = id - HOTKEY_ID_BASE;
            const auto& sounds = g_config.GetSounds();
            if (soundIndex >= 0 && soundIndex < (int)sounds.size()) {
                g_engine.PlaySoundFile(sounds[soundIndex].GetFullPath());
            }
        }
    }
    break;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_BTN_ADD) AddSoundDialog();
        else if (id == ID_BTN_REMOVE) RemoveSelectedSound();
        else if (id == ID_BTN_PLAY) PlaySelectedSound();
        else if (id == ID_BTN_STOP_ALL) g_engine.StopAllSounds();
        else if (id == ID_BTN_SET_HOTKEY) ToggleHotkeyRecording();
        else if (id == ID_TRAY_EXIT) DestroyWindow(hWnd);
        else if (id == ID_TRAY_OPEN) { ShowWindow(hWnd, SW_RESTORE); SetForegroundWindow(hWnd); }

        if (code == CBN_SELCHANGE) {
            if (id == ID_COMBO_MIC || id == ID_COMBO_CABLE || id == ID_COMBO_MONITOR) ApplyDeviceSelection();
        }
    }
    break;

    case WM_HSCROLL:
    {
        int micPos = (int)SendMessage(GetDlgItem(hWnd, ID_SLIDER_MIC), TBM_GETPOS, 0, 0);
        int sndPos = (int)SendMessage(GetDlgItem(hWnd, ID_SLIDER_SOUND), TBM_GETPOS, 0, 0);
        g_engine.SetMicVolume(micPos / 100.0f);
        g_config.SetMicVolume(micPos / 100.0f);
        g_engine.SetSoundVolume(sndPos / 100.0f);
        g_config.SetSoundVolume(sndPos / 100.0f);
    }
    break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_TRAY:
        if (lParam == WM_RBUTTONUP) {
            POINT p; GetCursorPos(&p);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN, L"Open Vibepad");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
        }
        else if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(hWnd, SW_RESTORE); SetForegroundWindow(hWnd);
        }
        break;

    case WM_DESTROY:
        DeleteObject(hFontNormal);
        UnregisterAllHotkeys(hWnd);
        SetupTrayIcon(hWnd, false);
        g_config.Save();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Local\\Vibepad_Instance_Mutex_v1");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExistingWnd = FindWindowW(L"VibepadClass", NULL);
        if (hExistingWnd) {
            ShowWindow(hExistingWnd, SW_RESTORE);
            SetForegroundWindow(hExistingWnd);
        }
        return 0;
    }

    if (!Utils::IsVBCableInstalled()) {
        int msgId = MessageBoxW(NULL, L"VB-Audio Virtual Cable driver is not installed.\nInstall it now?", L"Driver Missing", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON1);
        if (msgId == IDYES) {
            Utils::InstallVBCable();
            return 0;
        }
    }

    g_config.Load();

    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;

    HICON hFileIcon = (HICON)LoadImageW(NULL, L"resources\\app.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    if (!hFileIcon) hFileIcon = LoadIcon(NULL, IDI_APPLICATION);

    wcex.hIcon = hFileIcon;
    wcex.hIconSm = hFileIcon;

    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"VibepadClass";
    RegisterClassExW(&wcex);

    hMainWnd = CreateWindowW(L"VibepadClass", L"Vibepad",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 610, 580, NULL, NULL, hInstance, NULL);

    if (!hMainWnd) return FALSE;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_engine.Shutdown();
    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);
    return (int)msg.wParam;
}