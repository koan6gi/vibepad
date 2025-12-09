#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <algorithm>
#include <vector>

#include "AudioEngine.h"
#include "Utils.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../lib/miniaudio.h"

const int SAMPLE_RATE = 48000;
const int CHANNELS = 2;

// -----------------------------------------------------------------------------
// HELPERS
// -----------------------------------------------------------------------------
ma_device_id* FindDeviceID(ma_device_info* infoList, ma_uint32 count, const std::string& targetName) {
    if (targetName.empty()) return NULL;
    for (ma_uint32 i = 0; i < count; ++i) {
        if (targetName == infoList[i].name) return &infoList[i].id;
    }
    return NULL;
}

// -----------------------------------------------------------------------------
// CALLBACK WRAPPERS
// -----------------------------------------------------------------------------
void DataCallback_Capture(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    auto* engine = (AudioEngine*)pDevice->pUserData;
    if (engine) engine->OnCapture(pInput, frameCount);
}

void DataCallback_Cable(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    auto* engine = (AudioEngine*)pDevice->pUserData;
    if (engine) engine->OnCableProcess(pOutput, frameCount);
}

void DataCallback_Monitor(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    auto* engine = (AudioEngine*)pDevice->pUserData;
    if (engine) engine->OnMonitorProcess(pOutput, frameCount);
}

// -----------------------------------------------------------------------------
// AUDIO ENGINE IMPLEMENTATION
// -----------------------------------------------------------------------------
AudioEngine::AudioEngine() {
    m_pContext = new ma_context();
    m_pCaptureDevice = new ma_device();
    m_pCableDevice = new ma_device();
    m_pMonitorDevice = new ma_device();
    m_pMicBuffer = new ma_rb();

    ma_context_init(NULL, 0, NULL, m_pContext);
    RefreshDeviceList();
}

AudioEngine::~AudioEngine() {
    Shutdown();
    delete (ma_rb*)m_pMicBuffer;
    delete m_pMonitorDevice;
    delete m_pCableDevice;
    delete m_pCaptureDevice;
    ma_context_uninit(m_pContext);
    delete m_pContext;
}

bool AudioEngine::Init(const std::string& inputDeviceName, const std::string& outputDeviceName, const std::string& monitorDeviceName) {
    if (m_isInitialized) Shutdown();

    // 1. Buffer Setup (100ms for Low Latency)
    size_t frameSizeInBytes = sizeof(float) * CHANNELS;
    size_t bufferSizeInFrames = (size_t)(SAMPLE_RATE * 0.1f);
    size_t bufferSizeInBytes = bufferSizeInFrames * frameSizeInBytes;

    // Allocate and store pointer to free it later
    m_pAudioBufferData = ma_malloc(bufferSizeInBytes, NULL);
    memset(m_pAudioBufferData, 0, bufferSizeInBytes);

    ma_rb* rb = (ma_rb*)m_pMicBuffer;
    if (ma_rb_init(bufferSizeInBytes, m_pAudioBufferData, NULL, rb) != MA_SUCCESS) {
        ma_free(m_pAudioBufferData, NULL);
        return false;
    }

    // 2. Resolve Device IDs
    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    ma_context_get_devices(m_pContext, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount);

    ma_device_id* pInputID = FindDeviceID(pCaptureInfos, captureCount, inputDeviceName);
    ma_device_id* pCableID = FindDeviceID(pPlaybackInfos, playbackCount, outputDeviceName);
    ma_device_id* pMonitorID = FindDeviceID(pPlaybackInfos, playbackCount, monitorDeviceName);

    // 3. Configure DEVICES
    // Capture
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.pDeviceID = pInputID;
    config.capture.format = ma_format_f32;
    config.capture.channels = CHANNELS;
    config.sampleRate = SAMPLE_RATE;
    config.dataCallback = DataCallback_Capture;
    config.pUserData = this;
    config.performanceProfile = ma_performance_profile_low_latency;

    if (ma_device_init(m_pContext, &config, m_pCaptureDevice) != MA_SUCCESS) {
        config.capture.pDeviceID = NULL;
        ma_device_init(m_pContext, &config, m_pCaptureDevice);
    }

    // Cable Output
    config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = pCableID;
    config.playback.format = ma_format_f32;
    config.playback.channels = CHANNELS;
    config.sampleRate = SAMPLE_RATE;
    config.dataCallback = DataCallback_Cable;
    config.pUserData = this;
    config.performanceProfile = ma_performance_profile_low_latency;

    ma_device_init(m_pContext, &config, m_pCableDevice);

    // Monitor Output
    config.playback.pDeviceID = pMonitorID;
    config.dataCallback = DataCallback_Monitor;
    config.performanceProfile = ma_performance_profile_low_latency;

    ma_device_init(m_pContext, &config, m_pMonitorDevice);

    // 4. Start
    ma_device_start(m_pCaptureDevice);
    ma_device_start(m_pCableDevice);
    if (ma_device_get_state(m_pMonitorDevice) == ma_device_state_started ||
        ma_device_get_state(m_pMonitorDevice) == ma_device_state_stopped) {
        ma_device_start(m_pMonitorDevice);
    }

    m_isInitialized = true;
    return true;
}

void AudioEngine::Shutdown() {
    if (!m_isInitialized) return;

    ma_device_uninit(m_pCaptureDevice);
    ma_device_uninit(m_pCableDevice);
    ma_device_uninit(m_pMonitorDevice);

    ma_rb_uninit((ma_rb*)m_pMicBuffer);

    // Free the raw buffer memory we allocated
    if (m_pAudioBufferData) {
        ma_free(m_pAudioBufferData, NULL);
        m_pAudioBufferData = nullptr;
    }

    m_isInitialized = false;
}

void AudioEngine::RefreshDeviceList() {
    m_inputDevices.clear();
    m_outputDevices.clear();
    ma_device_info* pP; ma_uint32 cP;
    ma_device_info* pC; ma_uint32 cC;
    if (ma_context_get_devices(m_pContext, &pP, &cP, &pC, &cC) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < cC; ++i) m_inputDevices.push_back({ pC[i].name, "" });
        for (ma_uint32 i = 0; i < cP; ++i) m_outputDevices.push_back({ pP[i].name, "" });
    }
}
std::vector<DeviceInfo> AudioEngine::GetInputDevices() { return m_inputDevices; }
std::vector<DeviceInfo> AudioEngine::GetOutputDevices() { return m_outputDevices; }

// -----------------------------------------------------------------------------
// PLAYBACK LOGIC
// -----------------------------------------------------------------------------
void AudioEngine::PlaySoundFile(const std::wstring& fullPath) {
    std::string pathUtf8 = Utils::WideToUtf8(fullPath);
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, CHANNELS, SAMPLE_RATE);
    if (ma_decoder_init_file(pathUtf8.c_str(), &config, &decoder) != MA_SUCCESS) return;

    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
    if (totalFrames == 0) totalFrames = 1024 * 1024;

    std::vector<float> tempBuffer(totalFrames * CHANNELS);
    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(&decoder, tempBuffer.data(), totalFrames, &framesRead);
    tempBuffer.resize(framesRead * CHANNELS);
    ma_decoder_uninit(&decoder);

    if (framesRead > 0) {
        auto audio = std::make_shared<AudioData>();
        audio->samples = std::move(tempBuffer);
        std::lock_guard<std::mutex> lock(m_soundMutex);

        ActiveSound sound;
        sound.data = audio;
        sound.cursorCable = 0;
        sound.cursorMonitor = 0;
        sound.finished = false;

        m_activeSounds.push_back(sound);
    }
}

void AudioEngine::StopAllSounds() {
    std::lock_guard<std::mutex> lock(m_soundMutex);
    m_activeSounds.clear();
}
void AudioEngine::SetMicVolume(float volume) { m_micVolume = volume; }
void AudioEngine::SetSoundVolume(float volume) { m_soundVolume = volume; }

// -----------------------------------------------------------------------------
// REAL-TIME AUDIO PROCESSING
// -----------------------------------------------------------------------------

void AudioEngine::OnCapture(const void* pInput, unsigned int frameCount) {
    ma_rb* rb = (ma_rb*)m_pMicBuffer;
    size_t sizeInBytes = frameCount * CHANNELS * sizeof(float);

    // Anti-Lag: Drop old data if buffer gets clogged
    size_t bytesInRB = ma_rb_available_read(rb);
    size_t latencyThreshold = (size_t)(SAMPLE_RATE * 0.05f) * CHANNELS * sizeof(float);

    if (bytesInRB > latencyThreshold) {
        size_t bytesToSkip = bytesInRB - latencyThreshold;
        ma_rb_seek_read(rb, bytesToSkip);
    }

    void* pWriteBuf = nullptr;
    size_t sizeAvailable = sizeInBytes;

    if (ma_rb_acquire_write(rb, &sizeAvailable, &pWriteBuf) == MA_SUCCESS) {
        size_t bytesToCopy = (sizeInBytes < sizeAvailable) ? sizeInBytes : sizeAvailable;
        memcpy(pWriteBuf, pInput, bytesToCopy);
        ma_rb_commit_write(rb, bytesToCopy);
    }
}

void AudioEngine::OnCableProcess(void* pOutput, unsigned int frameCount) {
    float* pOutF32 = (float*)pOutput;
    ma_rb* rb = (ma_rb*)m_pMicBuffer;

    // 1. Music (Cable Cursor)
    memset(pOutF32, 0, frameCount * CHANNELS * sizeof(float));
    MixSounds(pOutF32, frameCount, false);

    // 2. Mic (Immediate Read)
    size_t bytesNeeded = frameCount * CHANNELS * sizeof(float);
    size_t bytesAvailable = ma_rb_available_read(rb);

    if (bytesAvailable > 0) {
        size_t bytesToRead = (bytesAvailable < bytesNeeded) ? bytesAvailable : bytesNeeded;
        void* pBufferOut;

        if (ma_rb_acquire_read(rb, &bytesToRead, &pBufferOut) == MA_SUCCESS) {
            float* pMicData = (float*)pBufferOut;
            float micVol = m_micVolume;

            size_t floatsToRead = bytesToRead / sizeof(float);
            for (size_t i = 0; i < floatsToRead; ++i) {
                pOutF32[i] += pMicData[i] * micVol;
            }
            ma_rb_commit_read(rb, bytesToRead);
        }
    }
}

void AudioEngine::OnMonitorProcess(void* pOutput, unsigned int frameCount) {
    // Music (Monitor Cursor)
    memset(pOutput, 0, frameCount * CHANNELS * sizeof(float));
    MixSounds((float*)pOutput, frameCount, true);
}

void AudioEngine::MixSounds(float* pOutput, unsigned int frameCount, bool isMonitor) {
    std::unique_lock<std::mutex> lock(m_soundMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;

    float vol = m_soundVolume;

    for (auto it = m_activeSounds.begin(); it != m_activeSounds.end(); ) {
        ActiveSound& sound = *it;
        float* rawAudio = sound.data->samples.data();
        size_t totalSamples = sound.data->samples.size();

        size_t* pCursor = isMonitor ? &sound.cursorMonitor : &sound.cursorCable;

        for (unsigned int i = 0; i < frameCount; ++i) {
            for (int c = 0; c < CHANNELS; ++c) {
                if (*pCursor < totalSamples) {
                    pOutput[i * CHANNELS + c] += rawAudio[*pCursor] * vol;
                    (*pCursor)++;
                }
            }
        }

        if (sound.cursorCable >= totalSamples && sound.cursorMonitor >= totalSamples) {
            it = m_activeSounds.erase(it);
        }
        else {
            ++it;
        }
    }
}