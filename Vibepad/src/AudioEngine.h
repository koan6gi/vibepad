#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <memory>

// Forward declarations
struct ma_context;
struct ma_device;

struct AudioData {
    std::vector<float> samples;
    unsigned int channels = 2;
    unsigned int sampleRate = 48000;
};

struct ActiveSound {
    std::shared_ptr<AudioData> data;
    size_t cursorCable = 0;
    size_t cursorMonitor = 0;
    bool finished = false;
};

struct DeviceInfo {
    std::string name;
    std::string id;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool Init(const std::string& inputDeviceId,
        const std::string& outputDeviceId,
        const std::string& monitorDeviceId);

    void Shutdown();

    void RefreshDeviceList();
    std::vector<DeviceInfo> GetInputDevices();
    std::vector<DeviceInfo> GetOutputDevices();

    void PlaySoundFile(const std::wstring& fullPath);
    void StopAllSounds();

    void SetMicVolume(float volume);
    void SetSoundVolume(float volume);

    void OnCapture(const void* pInput, unsigned int frameCount);
    void OnCableProcess(void* pOutput, unsigned int frameCount);
    void OnMonitorProcess(void* pOutput, unsigned int frameCount);

private:
    void MixSounds(float* pOutput, unsigned int frameCount, bool isMonitor);

    ma_context* m_pContext = nullptr;
    ma_device* m_pCaptureDevice = nullptr;
    ma_device* m_pCableDevice = nullptr;
    ma_device* m_pMonitorDevice = nullptr;

    void* m_pMicBuffer = nullptr;       // Структура кольцевого буфера
    void* m_pAudioBufferData = nullptr; // Сырые данные буфера (для очистки памяти)

    bool m_isInitialized = false;

    std::atomic<float> m_micVolume{ 1.0f };
    std::atomic<float> m_soundVolume{ 1.0f };

    std::vector<ActiveSound> m_activeSounds;
    std::mutex m_soundMutex;

    std::vector<DeviceInfo> m_inputDevices;
    std::vector<DeviceInfo> m_outputDevices;
};