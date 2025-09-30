#ifndef FLUTTER_PLUGIN_WINDOWS_LOOPBACK_RECORDER_PLUGIN_H_
#define FLUTTER_PLUGIN_WINDOWS_LOOPBACK_RECORDER_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

// Prevent Windows.h from defining min/max macros that conflict with std::min/std::max
#define NOMINMAX

// Windows Audio API includes
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <combaseapi.h>

namespace windows_loopback_recorder {

enum class RecordingState {
  IDLE = 0,
  RECORDING = 1,
  PAUSED = 2
};

struct AudioConfig {
  UINT32 sampleRate = 44100;
  UINT32 channels = 2;
  UINT32 bitsPerSample = 16;
};

class WindowsLoopbackRecorderPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  WindowsLoopbackRecorderPlugin();

  virtual ~WindowsLoopbackRecorderPlugin();

  // Disallow copy and assign.
  WindowsLoopbackRecorderPlugin(const WindowsLoopbackRecorderPlugin&) = delete;
  WindowsLoopbackRecorderPlugin& operator=(const WindowsLoopbackRecorderPlugin&) = delete;

  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

 private:
  // Audio recording methods
  bool StartRecording(const AudioConfig& config);
  bool PauseRecording();
  bool ResumeRecording();
  bool StopRecording();
  RecordingState GetRecordingState();
  bool HasMicrophonePermission();
  bool RequestMicrophonePermission();
  std::vector<std::string> GetAvailableDevices();

  // WASAPI helper methods
  HRESULT InitializeSystemAudioCapture();
  HRESULT InitializeMicrophoneCapture();
  void CaptureThreadFunction();
  void MixAudioBuffers(const BYTE* systemBuffer, const BYTE* micBuffer,
                       UINT32 systemFrames, UINT32 micFrames,
                       std::vector<BYTE>& outputBuffer);

  // Audio capture thread management
  std::thread captureThread_;
  std::atomic<bool> shouldStop_{false};
  std::atomic<RecordingState> currentState_{RecordingState::IDLE};

  // WASAPI interfaces
  IMMDeviceEnumerator* deviceEnumerator_ = nullptr;
  IMMDevice* systemDevice_ = nullptr;
  IMMDevice* micDevice_ = nullptr;
  IAudioClient* systemAudioClient_ = nullptr;
  IAudioClient* micAudioClient_ = nullptr;
  IAudioCaptureClient* systemCaptureClient_ = nullptr;
  IAudioCaptureClient* micCaptureClient_ = nullptr;

  // Audio format configuration
  WAVEFORMATEX* systemWaveFormat_ = nullptr;
  WAVEFORMATEX* micWaveFormat_ = nullptr;
  AudioConfig audioConfig_;

  // Event stream for sending audio data to Dart
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> eventSink_ = nullptr;
  std::mutex eventSinkMutex_;

  // COM initialization
  bool comInitialized_ = false;
};

}  // namespace windows_loopback_recorder

#endif  // FLUTTER_PLUGIN_WINDOWS_LOOPBACK_RECORDER_PLUGIN_H_
