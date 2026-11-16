#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <atomic>
#include <thread>
#include <cmath>
#include "audio_detector.h"

static std::atomic<float> s_level{0.0f};
static std::atomic<bool> s_running{false};
static std::thread s_workerThread;

static void AudioWorker() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IAudioCaptureClient* pCaptureClient = NULL;
    WAVEFORMATEX* pwfx = NULL;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) goto cleanup;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) goto cleanup;

    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) goto cleanup;

    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) goto cleanup;

    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                  AUDCLNT_STREAMFLAGS_LOOPBACK,
                                  10000000, 0, pwfx, NULL);
    if (FAILED(hr)) goto cleanup;

    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    if (FAILED(hr)) goto cleanup;

    hr = pAudioClient->Start();
    if (FAILED(hr)) goto cleanup;

    while (s_running) {
        UINT32 packetLength = 0;
        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr) || packetLength == 0) {
            float cur = s_level.load();
            s_level.store(cur * 0.85f);
            Sleep(8);
            continue;
        }

        BYTE* pData = NULL;
        UINT32 numFramesAvailable = 0;
        DWORD flags = 0;

        hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
        if (SUCCEEDED(hr)) {
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && numFramesAvailable > 0) {
                float sumSq = 0.0f;
                int totalSamples = numFramesAvailable * pwfx->nChannels;

                if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                    (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                     ((WAVEFORMATEXTENSIBLE*)pwfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
                    float* samples = (float*)pData;
                    for (int i = 0; i < totalSamples; i++) {
                        sumSq += samples[i] * samples[i];
                    }
                } else if (pwfx->wBitsPerSample == 16) {
                    short* samples = (short*)pData;
                    for (int i = 0; i < totalSamples; i++) {
                        float s = samples[i] / 32768.0f;
                        sumSq += s * s;
                    }
                }

                float rms = sqrtf(sumSq / totalSamples);
                float normalized = rms * 6.0f;
                if (normalized > 1.0f) normalized = 1.0f;

                float prev = s_level.load();
                float smoothed = prev * 0.30f + normalized * 0.70f;
                s_level.store(smoothed);
            } else {
                float cur = s_level.load();
                s_level.store(cur * 0.85f);
            }
            pCaptureClient->ReleaseBuffer(numFramesAvailable);
        }
        Sleep(8);
    }

    pAudioClient->Stop();

cleanup:
    if (pwfx) CoTaskMemFree(pwfx);
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    CoUninitialize();
}

bool AudioDetector::Init() {
    if (s_running) return true;
    s_running = true;
    s_workerThread = std::thread(AudioWorker);
    return true;
}

void AudioDetector::Shutdown() {
    if (!s_running) return;
    s_running = false;
    if (s_workerThread.joinable()) {
        s_workerThread.join();
    }
}

float AudioDetector::GetLevel() {
    return s_level.load();
}
