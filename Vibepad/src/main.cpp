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

// GLOBALS
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
// HOTKEY UTILS
// -----------------------------------------------------------------------------

void UnregisterAllHotkeys(HWND hWnd) {
    if (hWnd == NULL) return;

    UnregisterHotKey(hWnd, HOTKEY_ID_PANIC);

    for (int i = 0; i < 1000; ++i) {
        UnregisterHotKey(hWnd, HOTKEY_ID_BASE + i);
    }
}

void RegisterConfigHotkeys(HWND hWndOverride = NULL) {
    HWND targetWnd = (hWndOverride != NULL) ? hWndOverride : hMainWnd;
    if (targetWnd == NULL) return;

    UnregisterAllHotkeys(targetWnd);

    RegisterHotKey(targetWnd, HOTKEY_ID_PANIC, MOD_ALT | 0x4000, VK_BACK);

    const auto& sounds = g_config.GetSounds();
    for (int i = 0; i < (int)sounds.size(); ++i) {
        if (sounds[i].hotkey > 0) {
            RegisterHotKey(targetWnd, HOTKEY_ID_BASE + i,
                sounds[i].modifiers | 0x4000,
                sounds[i].hotkey);
        }
    }
}

std::wstring GetKeyString(int vk, int mods) {
    if (vk == 0) return L"None";

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
        case VK_UP:       ss << L"Up"; break;
        case VK_DOWN:     ss << L"Down"; break;
        case VK_LEFT:     ss << L"Left"; break;
        case VK_RIGHT:    ss << L"Right"; break;
        case VK_HOME:     ss << L"Home"; break;
        case VK_END:      ss << L"End"; break;
        case VK_PRIOR:    ss << L"PgUp"; break;
        case VK_NEXT:     ss << L"PgDn"; break;
        case VK_INSERT:   ss << L"Insert"; break;
        case VK_DELETE:   ss << L"Delete"; break;
        case VK_SPACE:    ss << L"Space"; break;
        case VK_BACK:     ss << L"Backspace"; break;
        case VK_RETURN:   ss << L"Enter"; break;
        case VK_OEM_3:    ss << L"~"; break;
        default: ss << L"Key " << vk; break;
        }
    }
    return ss.str();
}

// -----------------------------------------------------------------------------
// UI HELPERS
// -----------------------------------------------------------------------------

void RefreshSoundList() {
    ListView_DeleteAllItems(hList);
    const auto& sounds = g_config.GetSounds();
    LVITEMW li = { 0 };
    li.mask = LVIF_TEXT | LVIF_PARAM;
    for (int i = 0; i < (int)sounds.size(); ++i) {
        li.iItem = i;
        li.iSubItem = 0;
        li.lParam = i;
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
        else MessageBoxW(hMainWnd, L"Failed to add sound.", L"Error", MB_ICONERROR);
    }
}

void PlaySelectedSound() {
    int iPos = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (iPos != -1) {
        LVITEM li = { 0 };
        li.iItem = iPos;
        li.mask = LVIF_PARAM;
        ListView_GetItem(hList, &li);
        const auto& sounds = g_config.GetSounds();
        if ((int)li.lParam < (int)sounds.size())
            g_engine.PlaySoundFile(sounds[li.lParam].GetFullPath());
    }
}

void RemoveSelectedSound() {
    int iPos = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (iPos != -1) {
        LVITEM li = { 0 };
        li.iItem = iPos;
        li.mask = LVIF_PARAM;
        ListView_GetItem(hList, &li);
        g_config.RemoveSound((int)li.lParam);
        RefreshSoundList();
        RegisterConfigHotkeys();
    }
}

void ToggleHotkeyRecording() {
    if (g_isRecordingHotkey) {
        g_isRecordingHotkey = false;
        g_recordingIndex = -1;
        SetWindowTextW(hBtnSetHotkey, L"Set Hotkey");
        EnableWindow(hList, TRUE);

        RegisterConfigHotkeys();
        return;
    }

    int iPos = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (iPos == -1) {
        MessageBoxW(hMainWnd, L"Please select a sound first.", L"Info", MB_ICONINFORMATION);
        return;
    }

    LVITEM li = { 0 };
    li.iItem = iPos;
    li.mask = LVIF_PARAM;
    ListView_GetItem(hList, &li);
    g_recordingIndex = (int)li.lParam;

    UnregisterAllHotkeys(hMainWnd);

    g_isRecordingHotkey = true;
    SetWindowTextW(hBtnSetHotkey, L"Press keys...");
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
    nid.hIcon = (HICON)LoadImage(NULL, IDI_INFORMATION, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    wcscpy_s(nid.szTip, L"Vibepad");

    if (add) Shell_NotifyIcon(NIM_ADD, &nid);
    else Shell_NotifyIcon(NIM_DELETE, &nid);
}

// -----------------------------------------------------------------------------
// WINDOW PROCEDURE
// -----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

    // HOTKEY RECORDING LOGIC (Intercept Keys)
    if (g_isRecordingHotkey) {
        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
            int vk = (int)wParam;

            if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_CAPITAL) {
                return DefWindowProc(hWnd, message, wParam, lParam);
            }

            int mods = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
            if (GetKeyState(VK_SHIFT) & 0x8000)   mods |= MOD_SHIFT;
            if (GetKeyState(VK_MENU) & 0x8000)    mods |= MOD_ALT;

            if (vk == VK_ESCAPE) {
                ToggleHotkeyRecording();
                return 0;
            }

            const auto& sounds = g_config.GetSounds();
            for (int i = 0; i < (int)sounds.size(); ++i) {
                if (i == g_recordingIndex) continue;

                if (sounds[i].hotkey == vk && sounds[i].modifiers == mods) {
                    std::wstring msg = L"This combination is already used by sound:\n\"" + sounds[i].name + L"\"";
                    MessageBoxW(hWnd, msg.c_str(), L"Duplicate Hotkey", MB_ICONWARNING);

                    g_isRecordingHotkey = false;
                    g_recordingIndex = -1;
                    SetWindowTextW(hBtnSetHotkey, L"Set Hotkey");
                    EnableWindow(hList, TRUE);

                    RegisterConfigHotkeys(hWnd);
                    return 0;
                }
            }

            g_config.SetSoundHotkey(g_recordingIndex, vk, mods);

            g_isRecordingHotkey = false;
            g_recordingIndex = -1;
            SetWindowTextW(hBtnSetHotkey, L"Set Hotkey");
            EnableWindow(hList, TRUE);

            RefreshSoundList();
            RegisterConfigHotkeys(hWnd);
            return 0;
        }
    }

    switch (message) {
    case WM_CREATE:
    {
        // 1. List
        hList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            10, 10, 460, 300, hWnd, (HMENU)ID_LIST_SOUNDS, NULL, NULL);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LVCOLUMNW lvc;
        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        lvc.iSubItem = 0; lvc.pszText = (LPWSTR)L"Sound Name"; lvc.cx = 280; lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(hList, 0, &lvc);
        lvc.iSubItem = 1; lvc.pszText = (LPWSTR)L"Hotkey"; lvc.cx = 160;
        ListView_InsertColumn(hList, 1, &lvc);

        // 2. Buttons
        CreateWindowW(L"BUTTON", L"Play", WS_CHILD | WS_VISIBLE, 480, 10, 100, 30, hWnd, (HMENU)ID_BTN_PLAY, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Stop (Alt+Bksp)", WS_CHILD | WS_VISIBLE | BS_FLAT, 480, 50, 100, 30, hWnd, (HMENU)ID_BTN_STOP_ALL, NULL, NULL);
        hBtnSetHotkey = CreateWindowW(L"BUTTON", L"Set Hotkey", WS_CHILD | WS_VISIBLE, 480, 90, 100, 30, hWnd, (HMENU)ID_BTN_SET_HOTKEY, NULL, NULL);

        CreateWindowW(L"BUTTON", L"Add Sound", WS_CHILD | WS_VISIBLE, 10, 320, 100, 30, hWnd, (HMENU)ID_BTN_ADD, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE, 120, 320, 100, 30, hWnd, (HMENU)ID_BTN_REMOVE, NULL, NULL);

        // 3. Sliders
        CreateWindowW(L"STATIC", L"Mic Vol:", WS_CHILD | WS_VISIBLE, 10, 360, 80, 20, hWnd, NULL, NULL, NULL);
        HWND hMicSlider = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 100, 360, 200, 30, hWnd, (HMENU)ID_SLIDER_MIC, NULL, NULL);
        SendMessage(hMicSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
        SendMessage(hMicSlider, TBM_SETPOS, TRUE, 100);

        CreateWindowW(L"STATIC", L"Sounds Vol:", WS_CHILD | WS_VISIBLE, 320, 360, 100, 20, hWnd, NULL, NULL, NULL);
        HWND hSndSlider = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 420, 360, 200, 30, hWnd, (HMENU)ID_SLIDER_SOUND, NULL, NULL);
        SendMessage(hSndSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
        SendMessage(hSndSlider, TBM_SETPOS, TRUE, 100);

        // 4. Combos
        CreateWindowW(L"STATIC", L"1. Input Device (Your Mic):", WS_CHILD | WS_VISIBLE, 10, 410, 200, 20, hWnd, NULL, NULL, NULL);
        hComboMic = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 10, 430, 400, 200, hWnd, (HMENU)ID_COMBO_MIC, NULL, NULL);

        CreateWindowW(L"STATIC", L"2. Output A (Select 'VB-Audio Cable'):", WS_CHILD | WS_VISIBLE, 10, 460, 300, 20, hWnd, NULL, NULL, NULL);
        hComboCable = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 10, 480, 400, 200, hWnd, (HMENU)ID_COMBO_CABLE, NULL, NULL);

        CreateWindowW(L"STATIC", L"3. Output B (Select Your Headphones):", WS_CHILD | WS_VISIBLE, 10, 510, 300, 20, hWnd, NULL, NULL, NULL);
        hComboMonitor = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 10, 530, 400, 200, hWnd, (HMENU)ID_COMBO_MONITOR, NULL, NULL);

        RefreshSoundList();
        PopulateDeviceCombos();
        ApplyDeviceSelection();
        SetupTrayIcon(hWnd, true);

        RegisterConfigHotkeys(hWnd);
    }
    break;

    // GLOBAL HOTKEY EVENT
    case WM_HOTKEY:
    {
        int id = (int)wParam;

        // Check for Panic Button
        if (id == HOTKEY_ID_PANIC) {
            g_engine.StopAllSounds();
        }
        else {
            // Check for Sounds
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
        else if (id == ID_TRAY_OPEN) {
            ShowWindow(hWnd, SW_RESTORE);
            SetForegroundWindow(hWnd);
        }

        if (code == CBN_SELCHANGE) {
            if (id == ID_COMBO_MIC || id == ID_COMBO_CABLE || id == ID_COMBO_MONITOR) {
                ApplyDeviceSelection();
            }
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
            ShowWindow(hWnd, SW_RESTORE);
            SetForegroundWindow(hWnd);
        }
        break;

    case WM_DESTROY:
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
    if (!Utils::IsVBCableInstalled()) {
        int msgId = MessageBoxW(NULL,
            L"VB-Audio Virtual Cable driver is not installed.\nVibepad requires it to function properly.\n\nInstall it now?",
            L"Driver Missing",
            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON1);

        if (msgId == IDYES) {
            Utils::InstallVBCable();
            MessageBoxW(NULL, L"Driver installer launched.\nPlease install the driver and RESTART your PC.\nThe application will now close.", L"Restart Required", MB_ICONINFORMATION);
            return 0;
        }
    }

    g_config.Load();

    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"VibepadClass";
    RegisterClassExW(&wcex);

    hMainWnd = CreateWindowW(L"VibepadClass", L"Vibepad (Alpha)",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 660, 620, NULL, NULL, hInstance, NULL);

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
    return (int)msg.wParam;
}