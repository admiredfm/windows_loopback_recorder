import 'dart:typed_data';
import 'windows_loopback_recorder_platform_interface.dart';

/// Export the API classes for external use
export 'windows_loopback_recorder_platform_interface.dart' show RecordingState, AudioConfig, VolumeData;

/// Windows Loopback Recorder Plugin
///
/// This plugin allows recording mixed audio from Windows system output (speakers)
/// and microphone input simultaneously using WASAPI.
class WindowsLoopbackRecorder {
  static WindowsLoopbackRecorderPlatform get _platform => WindowsLoopbackRecorderPlatform.instance;

  /// Get the platform version
  Future<String?> getPlatformVersion() {
    return _platform.getPlatformVersion();
  }

  /// Start recording system audio and microphone
  ///
  /// [config] - Audio configuration parameters (sample rate, channels, etc.)
  /// Returns true if recording started successfully
  Future<bool> startRecording({AudioConfig? config}) {
    return _platform.startRecording(config: config);
  }

  /// Pause the current recording
  ///
  /// Returns true if paused successfully
  Future<bool> pauseRecording() {
    return _platform.pauseRecording();
  }

  /// Resume recording from paused state
  ///
  /// Returns true if resumed successfully
  Future<bool> resumeRecording() {
    return _platform.resumeRecording();
  }

  /// Stop recording completely
  ///
  /// Returns true if stopped successfully
  Future<bool> stopRecording() {
    return _platform.stopRecording();
  }

  /// Get current recording state
  Future<RecordingState> getRecordingState() {
    return _platform.getRecordingState();
  }

  /// Check if microphone permission is granted
  Future<bool> hasMicrophonePermission() {
    return _platform.hasMicrophonePermission();
  }

  /// Request microphone permission from user
  Future<bool> requestMicrophonePermission() {
    return _platform.requestMicrophonePermission();
  }

  /// Get list of available audio devices
  Future<List<String>> getAvailableDevices() {
    return _platform.getAvailableDevices();
  }

  /// Get the audio data stream
  ///
  /// Returns a Stream of Uint8List containing mixed audio data
  /// from system output and microphone input
  Stream<Uint8List> get audioStream => _platform.audioStream;

  /// Get the actual audio format being used
  ///
  /// Returns the AudioConfig with actual parameters from the system
  Future<AudioConfig> getAudioFormat() {
    return _platform.getAudioFormat();
  }

  /// Start volume monitoring
  ///
  /// Begins monitoring the mixed audio volume (system + microphone)
  /// Returns true if volume monitoring started successfully
  Future<bool> startVolumeMonitoring() {
    return _platform.startVolumeMonitoring();
  }

  /// Stop volume monitoring
  ///
  /// Stops monitoring the audio volume
  /// Returns true if volume monitoring stopped successfully
  Future<bool> stopVolumeMonitoring() {
    return _platform.stopVolumeMonitoring();
  }

  /// Get the volume data stream
  ///
  /// Returns a Stream of VolumeData containing real-time volume information
  /// including RMS, decibels, and percentage values
  Stream<VolumeData> get volumeStream => _platform.volumeStream;
}
