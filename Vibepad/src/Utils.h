#pragma once

// WinAPI headers
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <shellapi.h>       // For ShellExecute (running installer)

// C++ headers
#include <string>
#include <filesystem>
#include <vector>

namespace Utils {

    // -------------------------------------------------------------------------
    // CONVERTERS (Required because JSON lib uses UTF-8, but WinAPI uses UTF-16)
    // -------------------------------------------------------------------------

    // Convert std::wstring (UTF-16) to std::string (UTF-8)
    inline std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    // Convert std::string (UTF-8) to std::wstring (UTF-16)
    inline std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    // -------------------------------------------------------------------------
    // DRIVER MANAGEMENT
    // -------------------------------------------------------------------------

    // Check if VB-Cable driver is installed via Windows Registry
    inline bool IsVBCableInstalled() {
        HKEY hKey;
        // VB-Audio usually stores keys here. 
        // We use RegOpenKeyExW explicitly for Unicode support.
        LONG lRes = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VB-Audio\\Cable", 0, KEY_READ, &hKey);

        if (lRes == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
        return false;
    }

    // Run the installer from the 'resources/vb_cable_driver' folder
    inline void InstallVBCable() {
        // 1. Define the directory where the driver files are located
        // Path: <AppDir>/resources/vb_cable_driver
        std::filesystem::path driverDir = std::filesystem::current_path() / "resources" / "vb_cable_driver";

        // 2. Define the full path to the executable
        std::filesystem::path installerPath = driverDir / "VBCABLE_Setup_x64.exe";

        // Check if file exists before trying to run it
        if (!std::filesystem::exists(installerPath)) {
            std::wstring errorMsg = L"Installer executable not found at:\n" + installerPath.wstring() +
                L"\n\nPlease ensure the 'resources/vb_cable_driver' folder contains the extracted VB-Cable files.";
            MessageBoxW(NULL, errorMsg.c_str(), L"File Missing", MB_ICONERROR);
            return;
        }

        // 3. Prepare structure to run the installer
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";               // Request Administrator privileges (Required for drivers)
        sei.lpFile = installerPath.c_str();  // The executable to run
        sei.lpDirectory = driverDir.c_str(); // IMPORTANT: Sets the working directory so the installer finds the .inf file
        sei.nShow = SW_SHOWNORMAL;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS; // Allows us to hold the process handle if needed

        // 4. Execute
        if (!ShellExecuteExW(&sei)) {
            // Logic if the user clicks "No" in UAC or another error occurs
            DWORD error = GetLastError();
            if (error == ERROR_CANCELLED) {
                MessageBoxW(NULL, L"Installation cancelled by user.", L"Cancelled", MB_ICONINFORMATION);
            }
            else {
                std::wstring errorCode = std::to_wstring(error);
                MessageBoxW(NULL, (L"Failed to start installer. Error Code: " + errorCode).c_str(), L"Error", MB_ICONERROR);
            }
        }
        else {
            // Successfully started. We can close the handle.
            // If you want to wait for installation to finish, you would use WaitForSingleObject here.
            if (sei.hProcess) {
                CloseHandle(sei.hProcess);
            }
        }
    }
}