#include "Config.h"
#include "Utils.h"
#include <fstream>
#include <iostream>
#include "../lib/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

std::wstring SoundEntry::GetFullPath() const {
    fs::path p = fs::current_path() / "sounds" / filename;
    return p.wstring();
}

ConfigManager::ConfigManager() {
    EnsureDirectories();
}

ConfigManager::~ConfigManager() {}

void ConfigManager::EnsureDirectories() {
    if (!fs::exists(SOUNDS_DIR)) fs::create_directory(SOUNDS_DIR);
}

void ConfigManager::Load() {
    if (!fs::exists(CONFIG_FILE)) return;

    try {
        std::ifstream file(CONFIG_FILE);
        if (!file.is_open()) return;
        json j;
        file >> j;

        m_inputDeviceId = j.value("input_device_id", "");
        m_outputDeviceId = j.value("output_device_id", "");
        m_monitorDeviceId = j.value("monitor_device_id", "");
        m_micVolume = j.value("mic_volume", 1.0f);
        m_soundVolume = j.value("sound_volume", 1.0f);

        m_sounds.clear();
        if (j.contains("sounds") && j["sounds"].is_array()) {
            for (const auto& item : j["sounds"]) {
                SoundEntry s;
                s.name = Utils::Utf8ToWide(item.value("name", "Unknown"));
                s.filename = Utils::Utf8ToWide(item.value("filename", ""));

                // Load Hotkeys
                s.hotkey = item.value("hotkey", 0);
                s.modifiers = item.value("modifiers", 0);

                if (!s.filename.empty()) m_sounds.push_back(s);
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "JSON Load Error: " << e.what() << std::endl;
    }
}

void ConfigManager::Save() {
    json j;
    j["input_device_id"] = m_inputDeviceId;
    j["output_device_id"] = m_outputDeviceId;
    j["monitor_device_id"] = m_monitorDeviceId;
    j["mic_volume"] = m_micVolume;
    j["sound_volume"] = m_soundVolume;

    j["sounds"] = json::array();
    for (const auto& s : m_sounds) {
        json sJson;
        sJson["name"] = Utils::WideToUtf8(s.name);
        sJson["filename"] = Utils::WideToUtf8(s.filename);
        sJson["hotkey"] = s.hotkey;
        sJson["modifiers"] = s.modifiers; // Save Modifiers
        j["sounds"].push_back(sJson);
    }

    try {
        std::ofstream file(CONFIG_FILE);
        file << std::setw(4) << j << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "JSON Save Error: " << e.what() << std::endl;
    }
}

bool ConfigManager::AddSound(const std::wstring& originalPath, const std::wstring& displayName) {
    fs::path sourcePath(originalPath);
    if (!fs::exists(sourcePath)) return false;

    std::wstring fileName = sourcePath.filename().wstring();
    fs::path destPath = fs::current_path() / SOUNDS_DIR / fileName;

    int counter = 1;
    while (fs::exists(destPath)) {
        std::wstring nameNoExt = sourcePath.stem().wstring();
        std::wstring ext = sourcePath.extension().wstring();
        fileName = nameNoExt + L"_" + std::to_wstring(counter) + ext;
        destPath = fs::current_path() / SOUNDS_DIR / fileName;
        counter++;
    }

    try {
        fs::copy_file(sourcePath, destPath, fs::copy_options::overwrite_existing);
    }
    catch (const std::exception& e) {
        std::cerr << "File Copy Error: " << e.what() << std::endl;
        return false;
    }

    SoundEntry newSound;
    newSound.name = displayName;
    newSound.filename = fileName;
    newSound.hotkey = 0;     // Default: No hotkey
    newSound.modifiers = 0;

    m_sounds.push_back(newSound);
    Save();
    return true;
}

void ConfigManager::RemoveSound(int index) {
    if (index < 0 || index >= m_sounds.size()) return;
    try {
        fs::path p = fs::path(m_sounds[index].GetFullPath());
        if (fs::exists(p)) fs::remove(p);
    }
    catch (...) {}

    m_sounds.erase(m_sounds.begin() + index);
    Save();
}

void ConfigManager::SetSoundHotkey(int index, int vkCode, int mods) {
    if (index >= 0 && index < m_sounds.size()) {
        m_sounds[index].hotkey = vkCode;
        m_sounds[index].modifiers = mods;
        Save();
    }
}

const std::vector<SoundEntry>& ConfigManager::GetSounds() const { return m_sounds; }

std::string ConfigManager::GetInputDeviceId() const { return m_inputDeviceId; }
void ConfigManager::SetInputDeviceId(const std::string& id) { m_inputDeviceId = id; }

std::string ConfigManager::GetOutputDeviceId() const { return m_outputDeviceId; }
void ConfigManager::SetOutputDeviceId(const std::string& id) { m_outputDeviceId = id; }

std::string ConfigManager::GetMonitorDeviceId() const { return m_monitorDeviceId; }
void ConfigManager::SetMonitorDeviceId(const std::string& id) { m_monitorDeviceId = id; }

float ConfigManager::GetMicVolume() const { return m_micVolume; }
void ConfigManager::SetMicVolume(float vol) { m_micVolume = vol; }

float ConfigManager::GetSoundVolume() const { return m_soundVolume; }
void ConfigManager::SetSoundVolume(float vol) { m_soundVolume = vol; }