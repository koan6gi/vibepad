#pragma once

#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <shellapi.h>       
#include <string>
#include <filesystem>
#include <vector>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace Utils {

    // -----------------------------------------------------------------------------
    // STRING CONVERTERS
    // -----------------------------------------------------------------------------

    inline std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    inline std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    // -----------------------------------------------------------------------------
    // DRIVER MANAGEMENT
    // -----------------------------------------------------------------------------

    inline bool IsVBCableInstalled() {
        // Check Output Devices
        int waveOutCount = waveOutGetNumDevs();
        for (int i = 0; i < waveOutCount; i++) {
            WAVEOUTCAPSW caps;
            if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                if (wcsstr(caps.szPname, L"CABLE") != NULL ||
                    wcsstr(caps.szPname, L"VB-Audio") != NULL) {
                    return true;
                }
            }
        }
        // Check Input Devices
        int waveInCount = waveInGetNumDevs();
        for (int i = 0; i < waveInCount; i++) {
            WAVEINCAPSW caps;
            if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                if (wcsstr(caps.szPname, L"CABLE") != NULL ||
                    wcsstr(caps.szPname, L"VB-Audio") != NULL) {
                    return true;
                }
            }
        }
        return false;
    }

    inline void InstallVBCable() {
        std::filesystem::path driverDir = std::filesystem::current_path() / "resources" / "vb_cable_driver";
        std::filesystem::path installerPath = driverDir / "VBCABLE_Setup_x64.exe";

        if (!std::filesystem::exists(installerPath)) {
            MessageBoxW(NULL,
                (L"Installer executable not found at:\n" + installerPath.wstring() + L"\n\nPlease ensure 'resources/vb_cable_driver' exists.").c_str(),
                L"File Missing", MB_ICONERROR);
            return;
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = installerPath.c_str();
        sei.lpDirectory = driverDir.c_str();
        sei.nShow = SW_SHOWNORMAL;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;

        if (!ShellExecuteExW(&sei)) {
            DWORD error = GetLastError();
            if (error != ERROR_CANCELLED) {
                MessageBoxW(NULL, (L"Failed to start installer. Error: " + std::to_wstring(error)).c_str(), L"Error", MB_ICONERROR);
            }
        }
        else {
            if (sei.hProcess) CloseHandle(sei.hProcess);
        }
    }
}