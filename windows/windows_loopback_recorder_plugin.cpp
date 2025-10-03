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
#include <cmath>
#include <chrono>

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

  auto volume_event_channel =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          registrar->messenger(), "windows_loopback_recorder/volume_stream",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<WindowsLoopbackRecorderPlugin>();

  // Set up audio event channel handler
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

  // Set up volume event channel handler
  auto volume_handler = std::make_unique<flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [plugin_pointer = plugin.get()](
          const flutter::EncodableValue* arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events)
          -> std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>> {
        {
          std::lock_guard<std::mutex> lock(plugin_pointer->volumeEventSinkMutex_);
          plugin_pointer->volumeEventSink_ = std::move(events);
          plugin_pointer->volumeMonitoringEnabled_ = true;
        }
        return nullptr;
      },
      [plugin_pointer = plugin.get()](const flutter::EncodableValue* arguments)
          -> std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>> {
        {
          std::lock_guard<std::mutex> lock(plugin_pointer->volumeEventSinkMutex_);
          plugin_pointer->volumeEventSink_.reset();
          plugin_pointer->volumeMonitoringEnabled_ = false;
        }
        return nullptr;
      });

  event_channel->SetStreamHandler(std::move(handler));
  volume_event_channel->SetStreamHandler(std::move(volume_handler));

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

  // Clean up resampler
  CleanupResampler();

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

    // Print system format for debugging
    if (systemWaveFormat_) {
      printf("System format: %dHz, %dch, %dbit\n",
             systemWaveFormat_->nSamplesPerSec,
             systemWaveFormat_->nChannels,
             systemWaveFormat_->wBitsPerSample);
    }

    // Return user-configured format (what we output after processing)
    format_info[flutter::EncodableValue("sampleRate")] = flutter::EncodableValue(static_cast<int32_t>(audioConfig_.sampleRate));
    format_info[flutter::EncodableValue("channels")] = flutter::EncodableValue(static_cast<int32_t>(audioConfig_.channels));
    format_info[flutter::EncodableValue("bitsPerSample")] = flutter::EncodableValue(static_cast<int32_t>(audioConfig_.bitsPerSample));

    printf("Returning user format: %dHz, %dch, %dbit\n",
           audioConfig_.sampleRate, audioConfig_.channels, audioConfig_.bitsPerSample);

    result->Success(flutter::EncodableValue(format_info));

  } else if (method_call.method_name() == "startVolumeMonitoring") {
    bool success = StartVolumeMonitoring();
    result->Success(flutter::EncodableValue(success));

  } else if (method_call.method_name() == "stopVolumeMonitoring") {
    bool success = StopVolumeMonitoring();
    result->Success(flutter::EncodableValue(success));

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

  // Initialize resampler based on user configuration
  if (!InitializeResampler()) {
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

  // Clean up resampler
  CleanupResampler();

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
            *outputSample = sample; // Preserve full system audio quality
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

            // Safe mixing: prevent overflow by using 32-bit arithmetic and clamping
            int32_t mixed = static_cast<int32_t>(*outputSample) + static_cast<int32_t>(micSample);
            if (mixed > 32767) mixed = 32767;
            if (mixed < -32768) mixed = -32768;
            *outputSample = static_cast<int16_t>(mixed);
          }
        }
      }
    }

    // Note: We don't update audioConfig_ here as it contains user preferences
    // The actual device format is stored in deviceConfig_

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
            // Preserve full dynamic range for better audio quality
            int16_t pcmSample = static_cast<int16_t>(floatSample * 32767.0f);
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

            // Safe mixing: prevent overflow by using 32-bit arithmetic and clamping
            int32_t mixed = static_cast<int32_t>(*outputSample) + static_cast<int32_t>(micSample);
            if (mixed > 32767) mixed = 32767;
            if (mixed < -32768) mixed = -32768;
            *outputSample = static_cast<int16_t>(mixed);
          }
        }
      }
    }

    // Note: We don't update audioConfig_ here as it contains user preferences
    // The actual device format is stored in deviceConfig_
  } else {
    // Unsupported format, just copy system audio
    if (systemBuffer && systemFrames > 0) {
      UINT32 sourceSize = systemFrames * bytesPerFrame;
      UINT32 copySize = (sourceSize < bufferSize) ? sourceSize : bufferSize;
      std::memcpy(outputBuffer.data(), systemBuffer, copySize);
    }
  }

  // Apply user-defined audio format processing (resampling, channel conversion)
  ProcessAudioFormat(outputBuffer);

  // Calculate and send volume update if monitoring is enabled
  if (volumeMonitoringEnabled_ && !outputBuffer.empty()) {
    double rms = CalculateRMS(outputBuffer);
    SendVolumeUpdate(rms);
  }
}

// Audio processing methods implementation
bool WindowsLoopbackRecorderPlugin::InitializeResampler() {
  // Clean up existing resampler if any
  CleanupResampler();

  // Check if resampling is needed
  if (!systemWaveFormat_) {
    return false;
  }

  // Store device configuration
  deviceConfig_.sampleRate = systemWaveFormat_->nSamplesPerSec;
  deviceConfig_.channels = systemWaveFormat_->nChannels;
  deviceConfig_.bitsPerSample = systemWaveFormat_->wBitsPerSample;

  // Check if resampling is necessary
  resamplingEnabled_ = (deviceConfig_.sampleRate != audioConfig_.sampleRate) ||
                      (deviceConfig_.channels != audioConfig_.channels);

  if (resamplingEnabled_) {
    // Create libsamplerate resampler for the target channel count
    int error;
    srcState_ = src_new(SRC_SINC_MEDIUM_QUALITY, audioConfig_.channels, &error);
    if (error != 0) {
      srcState_ = nullptr;
      return false;
    }
  }

  return true;
}

void WindowsLoopbackRecorderPlugin::CleanupResampler() {
  if (srcState_) {
    src_delete(srcState_);
    srcState_ = nullptr;
  }
  resamplingEnabled_ = false;
}

bool WindowsLoopbackRecorderPlugin::ProcessAudioFormat(std::vector<BYTE>& audioBuffer) {
  if (!resamplingEnabled_) {
    return true; // No processing needed
  }

  try {
    // Step 1: Convert channels if necessary
    if (deviceConfig_.channels != audioConfig_.channels) {
      audioBuffer = ConvertChannels(audioBuffer);
    }

    // Step 2: Resample if necessary
    if (deviceConfig_.sampleRate != audioConfig_.sampleRate) {
      audioBuffer = ResampleAudio(audioBuffer);
    }

    return true;
  } catch (...) {
    return false;
  }
}

std::vector<BYTE> WindowsLoopbackRecorderPlugin::ResampleAudio(const std::vector<BYTE>& inputBuffer) {
  if (!srcState_ || deviceConfig_.sampleRate == audioConfig_.sampleRate) {
    return inputBuffer;
  }

  // Convert to float for libsamplerate
  std::vector<float> floatInput = ConvertToFloat(inputBuffer);

  // Calculate output size
  double ratio = static_cast<double>(audioConfig_.sampleRate) / deviceConfig_.sampleRate;
  size_t inputFrames = floatInput.size() / audioConfig_.channels;
  size_t maxOutputFrames = static_cast<size_t>(inputFrames * ratio) + 1024;

  std::vector<float> floatOutput(maxOutputFrames * audioConfig_.channels);

  // Setup SRC data structure
  SRC_DATA srcData;
  srcData.data_in = floatInput.data();
  srcData.input_frames = inputFrames;
  srcData.data_out = floatOutput.data();
  srcData.output_frames = maxOutputFrames;
  srcData.src_ratio = ratio;
  srcData.end_of_input = 0;

  // Process resampling using libsamplerate
  int error = src_process(srcState_, &srcData);
  if (error != 0) {
    return inputBuffer; // Return original on error
  }

  // Resize to actual output
  floatOutput.resize(srcData.output_frames_gen * audioConfig_.channels);

  // Convert back to BYTE
  return ConvertFromFloat(floatOutput);
}

std::vector<BYTE> WindowsLoopbackRecorderPlugin::ConvertChannels(const std::vector<BYTE>& inputBuffer) {
  if (deviceConfig_.channels == audioConfig_.channels) {
    return inputBuffer;
  }

  // Convert bytes to int16 for processing
  size_t inputSamples = inputBuffer.size() / 2; // Assuming 16-bit samples
  size_t inputFrames = inputSamples / deviceConfig_.channels;

  const int16_t* inputData = reinterpret_cast<const int16_t*>(inputBuffer.data());

  std::vector<int16_t> outputData;

  if (deviceConfig_.channels == 2 && audioConfig_.channels == 1) {
    // Stereo to mono: mix left and right channels
    outputData.resize(inputFrames);
    for (size_t frame = 0; frame < inputFrames; frame++) {
      int32_t mixed = static_cast<int32_t>(inputData[frame * 2]) +
                     static_cast<int32_t>(inputData[frame * 2 + 1]);
      outputData[frame] = static_cast<int16_t>(mixed / 2);
    }
  } else if (deviceConfig_.channels == 1 && audioConfig_.channels == 2) {
    // Mono to stereo: duplicate mono channel
    outputData.resize(inputFrames * 2);
    for (size_t frame = 0; frame < inputFrames; frame++) {
      outputData[frame * 2] = inputData[frame];
      outputData[frame * 2 + 1] = inputData[frame];
    }
  } else {
    // For other channel conversions, just copy as much as possible
    size_t outputFrames = inputFrames;
    size_t outputSamples = outputFrames * audioConfig_.channels;
    outputData.resize(outputSamples, 0);

    size_t minChannels = (deviceConfig_.channels < audioConfig_.channels) ? deviceConfig_.channels : audioConfig_.channels;
    for (size_t frame = 0; frame < outputFrames; frame++) {
      for (size_t ch = 0; ch < minChannels; ch++) {
        outputData[frame * audioConfig_.channels + ch] =
            inputData[frame * deviceConfig_.channels + ch];
      }
    }
  }

  // Convert back to BYTE
  std::vector<BYTE> result(outputData.size() * 2);
  memcpy(result.data(), outputData.data(), result.size());
  return result;
}

// ConvertToFloat and ConvertFromFloat methods for libsamplerate compatibility
// libsamplerate processes audio in float format (-1.0 to 1.0)

std::vector<float> WindowsLoopbackRecorderPlugin::ConvertToFloat(const std::vector<BYTE>& byteBuffer) {
  size_t sampleCount = byteBuffer.size() / 2; // Assuming 16-bit samples
  std::vector<float> floatBuffer(sampleCount);

  const int16_t* int16Data = reinterpret_cast<const int16_t*>(byteBuffer.data());

  for (size_t i = 0; i < sampleCount; i++) {
    floatBuffer[i] = static_cast<float>(int16Data[i]) / 32768.0f;
  }

  return floatBuffer;
}

std::vector<BYTE> WindowsLoopbackRecorderPlugin::ConvertFromFloat(const std::vector<float>& floatBuffer) {
  std::vector<BYTE> byteBuffer(floatBuffer.size() * 2);
  int16_t* int16Data = reinterpret_cast<int16_t*>(byteBuffer.data());

  for (size_t i = 0; i < floatBuffer.size(); i++) {
    // Clamp and convert float to int16
    float sample = floatBuffer[i];
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    int16Data[i] = static_cast<int16_t>(sample * 32767.0f);
  }

  return byteBuffer;
}

// Volume monitoring methods implementation
double WindowsLoopbackRecorderPlugin::CalculateRMS(const std::vector<BYTE>& audioBuffer) {
  if (audioBuffer.empty()) {
    return 0.0;
  }

  // Convert BYTE buffer to 16-bit samples
  const int16_t* samples = reinterpret_cast<const int16_t*>(audioBuffer.data());
  size_t sampleCount = audioBuffer.size() / 2;

  if (sampleCount == 0) {
    return 0.0;
  }

  // Calculate RMS (Root Mean Square)
  double sum = 0.0;
  for (size_t i = 0; i < sampleCount; i++) {
    double sample = static_cast<double>(samples[i]) / 32768.0; // Normalize to [-1, 1]
    sum += sample * sample;
  }

  double rms = sqrt(sum / sampleCount);
  return rms;
}

double WindowsLoopbackRecorderPlugin::RMSToDecibels(double rms) {
  if (rms <= 0.0) {
    return -96.0; // Return -96dB for silence (practical minimum)
  }

  // Convert RMS to decibels: 20 * log10(rms)
  // Reference level: 1.0 RMS = 0 dB
  double db = 20.0 * log10(rms);

  // Clamp to reasonable range
  if (db < -96.0) db = -96.0;
  if (db > 0.0) db = 0.0;

  return db;
}

int WindowsLoopbackRecorderPlugin::DecibelsToPercentage(double db) {
  // Convert dB to percentage
  // -96dB = 0%, 0dB = 100%
  // Linear mapping: percentage = (db + 96) / 96 * 100

  if (db <= -96.0) return 0;
  if (db >= 0.0) return 100;

  int percentage = static_cast<int>((db + 96.0) / 96.0 * 100.0);

  // Ensure within bounds
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;

  return percentage;
}

void WindowsLoopbackRecorderPlugin::SendVolumeUpdate(double rms) {
  if (!volumeMonitoringEnabled_) {
    return;
  }

  std::lock_guard<std::mutex> lock(volumeEventSinkMutex_);
  if (volumeEventSink_) {
    double db = RMSToDecibels(rms);
    int percentage = DecibelsToPercentage(db);

    // Create volume data map
    flutter::EncodableMap volumeData;
    volumeData[flutter::EncodableValue("rms")] = flutter::EncodableValue(rms);
    volumeData[flutter::EncodableValue("db")] = flutter::EncodableValue(db);
    volumeData[flutter::EncodableValue("percentage")] = flutter::EncodableValue(percentage);
    volumeData[flutter::EncodableValue("timestamp")] = flutter::EncodableValue(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    volumeEventSink_->Success(flutter::EncodableValue(volumeData));
  }
}

bool WindowsLoopbackRecorderPlugin::StartVolumeMonitoring() {
  volumeMonitoringEnabled_ = true;
  return true;
}

bool WindowsLoopbackRecorderPlugin::StopVolumeMonitoring() {
  volumeMonitoringEnabled_ = false;
  return true;
}

}  // namespace windows_loopback_recorder
