#include "windows_loopback_recorder/windows_loopback_recorder_plugin.h"

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>

#include <memory>
#include <sstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace windows_loopback_recorder {

// static
void WindowsLoopbackRecorderPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "windows_loopback_recorder",
          &flutter::StandardMethodCodec::GetInstance());

  auto event_channel =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          registrar->messenger(), "windows_loopback_recorder/audio_stream",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<WindowsLoopbackRecorderPlugin>();

  // Set up event channel handler
  auto handler = std::make_unique<flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [plugin_pointer = plugin.get()](
          const flutter::EncodableValue* arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events)
          -> std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>> {
        {
          std::lock_guard<std::mutex> lock(plugin_pointer->eventSinkMutex_);
          plugin_pointer->eventSink_ = std::move(events);
        }
        return nullptr;
      },
      [plugin_pointer = plugin.get()](const flutter::EncodableValue* arguments)
          -> std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>> {
        {
          std::lock_guard<std::mutex> lock(plugin_pointer->eventSinkMutex_);
          plugin_pointer->eventSink_.reset();
        }
        return nullptr;
      });

  event_channel->SetStreamHandler(std::move(handler));

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

WindowsLoopbackRecorderPlugin::WindowsLoopbackRecorderPlugin() {
  // Initialize COM
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (SUCCEEDED(hr)) {
    comInitialized_ = true;

    // Create device enumerator
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                         CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                         (void**)&deviceEnumerator_);

    if (FAILED(hr)) {
      deviceEnumerator_ = nullptr;
    }
  }
}

WindowsLoopbackRecorderPlugin::~WindowsLoopbackRecorderPlugin() {
  StopRecording();

  // Clean up WASAPI resources
  if (systemCaptureClient_) {
    systemCaptureClient_->Release();
  }
  if (micCaptureClient_) {
    micCaptureClient_->Release();
  }
  if (systemAudioClient_) {
    systemAudioClient_->Release();
  }
  if (micAudioClient_) {
    micAudioClient_->Release();
  }
  if (systemDevice_) {
    systemDevice_->Release();
  }
  if (micDevice_) {
    micDevice_->Release();
  }
  if (deviceEnumerator_) {
    deviceEnumerator_->Release();
  }

  if (systemWaveFormat_) {
    CoTaskMemFree(systemWaveFormat_);
  }
  if (micWaveFormat_) {
    CoTaskMemFree(micWaveFormat_);
  }

  if (comInitialized_) {
    CoUninitialize();
  }
}

void WindowsLoopbackRecorderPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {

  if (method_call.method_name() == "getPlatformVersion") {
    std::ostringstream version_stream;
    version_stream << "Windows ";
    if (IsWindows10OrGreater()) {
      version_stream << "10+";
    } else if (IsWindows8OrGreater()) {
      version_stream << "8";
    } else if (IsWindows7OrGreater()) {
      version_stream << "7";
    }
    result->Success(flutter::EncodableValue(version_stream.str()));

  } else if (method_call.method_name() == "startRecording") {
    AudioConfig config;

    // Parse audio configuration if provided
    if (method_call.arguments() && !method_call.arguments()->IsNull()) {
      const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
      if (args) {
        auto sample_rate_it = args->find(flutter::EncodableValue("sampleRate"));
        if (sample_rate_it != args->end() && !sample_rate_it->second.IsNull()) {
          config.sampleRate = std::get<int32_t>(sample_rate_it->second);
        }

        auto channels_it = args->find(flutter::EncodableValue("channels"));
        if (channels_it != args->end() && !channels_it->second.IsNull()) {
          config.channels = std::get<int32_t>(channels_it->second);
        }

        auto bits_it = args->find(flutter::EncodableValue("bitsPerSample"));
        if (bits_it != args->end() && !bits_it->second.IsNull()) {
          config.bitsPerSample = std::get<int32_t>(bits_it->second);
        }
      }
    }

    bool success = StartRecording(config);
    result->Success(flutter::EncodableValue(success));

  } else if (method_call.method_name() == "pauseRecording") {
    bool success = PauseRecording();
    result->Success(flutter::EncodableValue(success));

  } else if (method_call.method_name() == "resumeRecording") {
    bool success = ResumeRecording();
    result->Success(flutter::EncodableValue(success));

  } else if (method_call.method_name() == "stopRecording") {
    bool success = StopRecording();
    result->Success(flutter::EncodableValue(success));

  } else if (method_call.method_name() == "getRecordingState") {
    int state = static_cast<int>(GetRecordingState());
    result->Success(flutter::EncodableValue(state));

  } else if (method_call.method_name() == "hasMicrophonePermission") {
    bool hasPermission = HasMicrophonePermission();
    result->Success(flutter::EncodableValue(hasPermission));

  } else if (method_call.method_name() == "requestMicrophonePermission") {
    bool granted = RequestMicrophonePermission();
    result->Success(flutter::EncodableValue(granted));

  } else if (method_call.method_name() == "getAvailableDevices") {
    auto devices = GetAvailableDevices();
    flutter::EncodableList device_list;
    for (const auto& device : devices) {
      device_list.push_back(flutter::EncodableValue(device));
    }
    result->Success(flutter::EncodableValue(device_list));

  } else if (method_call.method_name() == "getAudioFormat") {
    flutter::EncodableMap format_info;
    if (systemWaveFormat_) {
      // Always return 16-bit format since we convert everything to 16-bit
      format_info[flutter::EncodableValue("sampleRate")] = flutter::EncodableValue(static_cast<int32_t>(systemWaveFormat_->nSamplesPerSec));
      format_info[flutter::EncodableValue("channels")] = flutter::EncodableValue(static_cast<int32_t>(systemWaveFormat_->nChannels));
      format_info[flutter::EncodableValue("bitsPerSample")] = flutter::EncodableValue(16);
    } else {
      // Default format
      format_info[flutter::EncodableValue("sampleRate")] = flutter::EncodableValue(44100);
      format_info[flutter::EncodableValue("channels")] = flutter::EncodableValue(2);
      format_info[flutter::EncodableValue("bitsPerSample")] = flutter::EncodableValue(16);
    }
    result->Success(flutter::EncodableValue(format_info));

  } else {
    result->NotImplemented();
  }
}

bool WindowsLoopbackRecorderPlugin::StartRecording(const AudioConfig& config) {
  if (currentState_ != RecordingState::IDLE) {
    return false;
  }

  audioConfig_ = config;

  // Initialize audio capture
  if (FAILED(InitializeSystemAudioCapture()) || FAILED(InitializeMicrophoneCapture())) {
    return false;
  }

  // Start capture thread
  shouldStop_ = false;
  captureThread_ = std::thread(&WindowsLoopbackRecorderPlugin::CaptureThreadFunction, this);
  currentState_ = RecordingState::RECORDING;

  return true;
}

bool WindowsLoopbackRecorderPlugin::PauseRecording() {
  if (currentState_ != RecordingState::RECORDING) {
    return false;
  }

  currentState_ = RecordingState::PAUSED;
  return true;
}

bool WindowsLoopbackRecorderPlugin::ResumeRecording() {
  if (currentState_ != RecordingState::PAUSED) {
    return false;
  }

  currentState_ = RecordingState::RECORDING;
  return true;
}

bool WindowsLoopbackRecorderPlugin::StopRecording() {
  if (currentState_ == RecordingState::IDLE) {
    return true;
  }

  shouldStop_ = true;

  if (captureThread_.joinable()) {
    captureThread_.join();
  }

  // Stop audio clients
  if (systemAudioClient_) {
    systemAudioClient_->Stop();
  }
  if (micAudioClient_) {
    micAudioClient_->Stop();
  }

  currentState_ = RecordingState::IDLE;
  return true;
}

RecordingState WindowsLoopbackRecorderPlugin::GetRecordingState() {
  return currentState_;
}

bool WindowsLoopbackRecorderPlugin::HasMicrophonePermission() {
  // On Windows 10+, check microphone privacy settings
  // For simplicity, we'll assume permission is available
  // In a production app, you would check registry keys or use Windows Privacy APIs
  return true;
}

bool WindowsLoopbackRecorderPlugin::RequestMicrophonePermission() {
  // Windows handles microphone permission requests automatically when audio APIs are used
  // The first time an app tries to access the microphone, Windows will show a permission dialog
  return true;
}

std::vector<std::string> WindowsLoopbackRecorderPlugin::GetAvailableDevices() {
  std::vector<std::string> devices;

  if (!deviceEnumerator_) {
    return devices;
  }

  IMMDeviceCollection* deviceCollection = nullptr;
  HRESULT hr = deviceEnumerator_->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &deviceCollection);

  if (SUCCEEDED(hr)) {
    UINT count = 0;
    deviceCollection->GetCount(&count);

    for (UINT i = 0; i < count; i++) {
      IMMDevice* device = nullptr;
      if (SUCCEEDED(deviceCollection->Item(i, &device))) {
        IPropertyStore* propertyStore = nullptr;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &propertyStore))) {
          PROPVARIANT friendlyName;
          PropVariantInit(&friendlyName);

          if (SUCCEEDED(propertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName))) {
            if (friendlyName.vt == VT_LPWSTR) {
              // Convert wide string to narrow string
              int size_needed = WideCharToMultiByte(CP_UTF8, 0, friendlyName.pwszVal, -1, nullptr, 0, nullptr, nullptr);
              std::string device_name(size_needed - 1, 0);
              WideCharToMultiByte(CP_UTF8, 0, friendlyName.pwszVal, -1, &device_name[0], size_needed, nullptr, nullptr);
              devices.push_back(device_name);
            }
          }

          PropVariantClear(&friendlyName);
          propertyStore->Release();
        }
        device->Release();
      }
    }
    deviceCollection->Release();
  }

  return devices;
}

HRESULT WindowsLoopbackRecorderPlugin::InitializeSystemAudioCapture() {
  if (!deviceEnumerator_) {
    return E_FAIL;
  }

  // Get default render device for loopback capture
  HRESULT hr = deviceEnumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &systemDevice_);
  if (FAILED(hr)) {
    return hr;
  }

  // Activate audio client
  hr = systemDevice_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&systemAudioClient_);
  if (FAILED(hr)) {
    return hr;
  }

  // Get mix format
  hr = systemAudioClient_->GetMixFormat(&systemWaveFormat_);
  if (FAILED(hr)) {
    return hr;
  }

  // Initialize audio client in loopback mode
  hr = systemAudioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                     AUDCLNT_STREAMFLAGS_LOOPBACK,
                                     0, 0, systemWaveFormat_, nullptr);
  if (FAILED(hr)) {
    return hr;
  }

  // Get capture client
  hr = systemAudioClient_->GetService(__uuidof(IAudioCaptureClient), (void**)&systemCaptureClient_);
  if (FAILED(hr)) {
    return hr;
  }

  // Start the audio client
  hr = systemAudioClient_->Start();
  return hr;
}

HRESULT WindowsLoopbackRecorderPlugin::InitializeMicrophoneCapture() {
  if (!deviceEnumerator_) {
    return E_FAIL;
  }

  // Get default capture device (microphone)
  HRESULT hr = deviceEnumerator_->GetDefaultAudioEndpoint(eCapture, eConsole, &micDevice_);
  if (FAILED(hr)) {
    return hr;
  }

  // Activate audio client
  hr = micDevice_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&micAudioClient_);
  if (FAILED(hr)) {
    return hr;
  }

  // Get mix format
  hr = micAudioClient_->GetMixFormat(&micWaveFormat_);
  if (FAILED(hr)) {
    return hr;
  }

  // Initialize audio client
  hr = micAudioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, micWaveFormat_, nullptr);
  if (FAILED(hr)) {
    return hr;
  }

  // Get capture client
  hr = micAudioClient_->GetService(__uuidof(IAudioCaptureClient), (void**)&micCaptureClient_);
  if (FAILED(hr)) {
    return hr;
  }

  // Start the audio client
  hr = micAudioClient_->Start();
  return hr;
}

void WindowsLoopbackRecorderPlugin::CaptureThreadFunction() {
  while (!shouldStop_) {
    if (currentState_ == RecordingState::PAUSED) {
      Sleep(10);
      continue;
    }

    UINT32 systemPacketLength = 0;
    UINT32 micPacketLength = 0;

    // Get system audio data
    if (systemCaptureClient_) {
      systemCaptureClient_->GetNextPacketSize(&systemPacketLength);
    }

    // Get microphone audio data
    if (micCaptureClient_) {
      micCaptureClient_->GetNextPacketSize(&micPacketLength);
    }

    if (systemPacketLength > 0 || micPacketLength > 0) {
      BYTE* systemData = nullptr;
      BYTE* micData = nullptr;
      UINT32 systemFrames = 0;
      UINT32 micFrames = 0;
      DWORD systemFlags = 0;
      DWORD micFlags = 0;

      // Get system audio buffer
      if (systemPacketLength > 0 && systemCaptureClient_) {
        systemCaptureClient_->GetBuffer(&systemData, &systemFrames, &systemFlags, nullptr, nullptr);
      }

      // Get microphone audio buffer
      if (micPacketLength > 0 && micCaptureClient_) {
        micCaptureClient_->GetBuffer(&micData, &micFrames, &micFlags, nullptr, nullptr);
      }

      // Mix audio buffers and send to Dart
      if (systemData || micData) {
        std::vector<BYTE> mixedBuffer;
        MixAudioBuffers(systemData, micData, systemFrames, micFrames, mixedBuffer);

        if (!mixedBuffer.empty()) {
          std::lock_guard<std::mutex> lock(eventSinkMutex_);
          if (eventSink_) {
            // Convert std::vector<BYTE> to std::vector<uint8_t> for Flutter
            std::vector<uint8_t> flutterBuffer(mixedBuffer.begin(), mixedBuffer.end());
            eventSink_->Success(flutter::EncodableValue(flutterBuffer));
          }
        }
      }

      // Release buffers
      if (systemData && systemCaptureClient_) {
        systemCaptureClient_->ReleaseBuffer(systemFrames);
      }
      if (micData && micCaptureClient_) {
        micCaptureClient_->ReleaseBuffer(micFrames);
      }
    }

    Sleep(10); // Small delay to prevent high CPU usage
  }
}

void WindowsLoopbackRecorderPlugin::MixAudioBuffers(const BYTE* systemBuffer, const BYTE* micBuffer,
                                                   UINT32 systemFrames, UINT32 micFrames,
                                                   std::vector<BYTE>& outputBuffer) {
  if (!systemWaveFormat_) {
    return;
  }

  // Use the actual system audio format parameters
  UINT32 bytesPerFrame = systemWaveFormat_->nBlockAlign;
  UINT32 maxFrames = (systemFrames > micFrames) ? systemFrames : micFrames;
  UINT32 bufferSize = maxFrames * bytesPerFrame;

  outputBuffer.resize(bufferSize);
  std::fill(outputBuffer.begin(), outputBuffer.end(), BYTE(0));

  // Handle different audio formats
  if (systemWaveFormat_->wBitsPerSample == 16) {
    // 16-bit PCM
    UINT32 bytesPerSample = 2;

    // Mix system audio (if available)
    if (systemBuffer && systemFrames > 0) {
      for (UINT32 frame = 0; frame < systemFrames && frame < maxFrames; frame++) {
        for (UINT32 channel = 0; channel < systemWaveFormat_->nChannels; channel++) {
          UINT32 sampleIndex = frame * systemWaveFormat_->nChannels + channel;
          UINT32 byteIndex = sampleIndex * bytesPerSample;

          if (byteIndex + 1 < bufferSize) {
            int16_t sample = *reinterpret_cast<const int16_t*>(&systemBuffer[byteIndex]);
            int16_t* outputSample = reinterpret_cast<int16_t*>(&outputBuffer[byteIndex]);
            *outputSample = static_cast<int16_t>(sample / 2); // Start with system audio at half volume
          }
        }
      }
    }

    // Add microphone audio (if available)
    if (micBuffer && micFrames > 0 && micWaveFormat_) {
      UINT32 minChannels = (micWaveFormat_->nChannels < systemWaveFormat_->nChannels) ? micWaveFormat_->nChannels : systemWaveFormat_->nChannels;
      for (UINT32 frame = 0; frame < micFrames && frame < maxFrames; frame++) {
        for (UINT32 channel = 0; channel < minChannels; channel++) {
          UINT32 micSampleIndex = frame * micWaveFormat_->nChannels + channel;
          UINT32 micByteIndex = micSampleIndex * bytesPerSample;

          UINT32 outputSampleIndex = frame * systemWaveFormat_->nChannels + channel;
          UINT32 outputByteIndex = outputSampleIndex * bytesPerSample;

          if (micByteIndex + 1 < micFrames * micWaveFormat_->nBlockAlign &&
              outputByteIndex + 1 < bufferSize) {
            int16_t micSample = *reinterpret_cast<const int16_t*>(&micBuffer[micByteIndex]);
            int16_t* outputSample = reinterpret_cast<int16_t*>(&outputBuffer[outputByteIndex]);
            *outputSample = static_cast<int16_t>(*outputSample + micSample / 2); // Add mic at half volume
          }
        }
      }
    }

    // Update format info for 16-bit audio
    audioConfig_.sampleRate = systemWaveFormat_->nSamplesPerSec;
    audioConfig_.channels = systemWaveFormat_->nChannels;
    audioConfig_.bitsPerSample = 16;

  } else if (systemWaveFormat_->wBitsPerSample == 32) {
    // 32-bit float (common on modern Windows)
    // Convert to 16-bit for consistency
    UINT32 outputSize = maxFrames * systemWaveFormat_->nChannels * 2; // 16-bit output
    outputBuffer.resize(outputSize);

    if (systemBuffer && systemFrames > 0) {
      for (UINT32 frame = 0; frame < systemFrames && frame < maxFrames; frame++) {
        for (UINT32 channel = 0; channel < systemWaveFormat_->nChannels; channel++) {
          UINT32 floatSampleIndex = frame * systemWaveFormat_->nChannels + channel;
          UINT32 floatByteIndex = floatSampleIndex * 4; // 4 bytes per float

          UINT32 outputSampleIndex = frame * systemWaveFormat_->nChannels + channel;
          UINT32 outputByteIndex = outputSampleIndex * 2; // 2 bytes per 16-bit sample

          if (floatByteIndex + 3 < systemFrames * systemWaveFormat_->nBlockAlign &&
              outputByteIndex + 1 < outputBuffer.size()) {
            float floatSample = *reinterpret_cast<const float*>(&systemBuffer[floatByteIndex]);
            // Convert float (-1.0 to 1.0) to 16-bit PCM (-32768 to 32767)
            int16_t pcmSample = static_cast<int16_t>(floatSample * 32767.0f / 2.0f); // Half volume
            *reinterpret_cast<int16_t*>(&outputBuffer[outputByteIndex]) = pcmSample;
          }
        }
      }
    }

    // Add microphone (assuming 16-bit or also convert if float)
    if (micBuffer && micFrames > 0 && micWaveFormat_) {
      UINT32 minChannels = (micWaveFormat_->nChannels < systemWaveFormat_->nChannels) ? micWaveFormat_->nChannels : systemWaveFormat_->nChannels;
      for (UINT32 frame = 0; frame < micFrames && frame < maxFrames; frame++) {
        for (UINT32 channel = 0; channel < minChannels; channel++) {
          UINT32 outputSampleIndex = frame * systemWaveFormat_->nChannels + channel;
          UINT32 outputByteIndex = outputSampleIndex * 2;

          if (outputByteIndex + 1 < outputBuffer.size()) {
            int16_t micSample;

            if (micWaveFormat_->wBitsPerSample == 16) {
              UINT32 micSampleIndex = frame * micWaveFormat_->nChannels + channel;
              UINT32 micByteIndex = micSampleIndex * 2;
              if (micByteIndex + 1 < micFrames * micWaveFormat_->nBlockAlign) {
                micSample = *reinterpret_cast<const int16_t*>(&micBuffer[micByteIndex]);
              } else {
                micSample = 0;
              }
            } else if (micWaveFormat_->wBitsPerSample == 32) {
              UINT32 micSampleIndex = frame * micWaveFormat_->nChannels + channel;
              UINT32 micByteIndex = micSampleIndex * 4;
              if (micByteIndex + 3 < micFrames * micWaveFormat_->nBlockAlign) {
                float micFloatSample = *reinterpret_cast<const float*>(&micBuffer[micByteIndex]);
                micSample = static_cast<int16_t>(micFloatSample * 32767.0f);
              } else {
                micSample = 0;
              }
            } else {
              micSample = 0;
            }

            int16_t* outputSample = reinterpret_cast<int16_t*>(&outputBuffer[outputByteIndex]);
            *outputSample = static_cast<int16_t>(*outputSample + micSample / 2);
          }
        }
      }
    }

    // Update the format info for consistent 16-bit output
    audioConfig_.sampleRate = systemWaveFormat_->nSamplesPerSec;
    audioConfig_.channels = systemWaveFormat_->nChannels;
    audioConfig_.bitsPerSample = 16;
  } else {
    // Unsupported format, just copy system audio
    if (systemBuffer && systemFrames > 0) {
      UINT32 sourceSize = systemFrames * bytesPerFrame;
      UINT32 copySize = (sourceSize < bufferSize) ? sourceSize : bufferSize;
      std::memcpy(outputBuffer.data(), systemBuffer, copySize);
    }
  }
}

}  // namespace windows_loopback_recorder
