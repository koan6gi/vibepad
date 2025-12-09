#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct SoundEntry {
    std::wstring name;
    std::wstring filename;
    int hotkey = 0;
    int modifiers = 0;

    std::wstring GetFullPath() const;
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    void Load();
    void Save();

    bool AddSound(const std::wstring& originalPath, const std::wstring& displayName);
    void RemoveSound(int index);
    void SetSoundHotkey(int index, int vkCode, int mods);

    const std::vector<SoundEntry>& GetSounds() const;

    std::string GetInputDeviceId() const;
    void SetInputDeviceId(const std::string& id);

    std::string GetOutputDeviceId() const;
    void SetOutputDeviceId(const std::string& id);

    std::string GetMonitorDeviceId() const;
    void SetMonitorDeviceId(const std::string& id);

    float GetMicVolume() const;
    void SetMicVolume(float vol);

    float GetSoundVolume() const;
    void SetSoundVolume(float vol);

private:
    std::vector<SoundEntry> m_sounds;

    std::string m_inputDeviceId;
    std::string m_outputDeviceId;
    std::string m_monitorDeviceId;

    float m_micVolume = 1.0f;
    float m_soundVolume = 1.0f;

    const std::wstring SOUNDS_DIR = L"sounds";
    const std::wstring CONFIG_FILE = L"config.json";

    void EnsureDirectories();
};