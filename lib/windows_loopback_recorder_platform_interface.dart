import 'dart:typed_data';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'windows_loopback_recorder_method_channel.dart';

/// Recording states
enum RecordingState {
  idle,
  recording,
  paused,
}

/// Audio configuration parameters
class AudioConfig {
  final int sampleRate;
  final int channels;
  final int bitsPerSample;

  const AudioConfig({
    this.sampleRate = 44100,
    this.channels = 2,
    this.bitsPerSample = 16,
  });

  factory AudioConfig.fromMap(Map<String, dynamic> map) {
    return AudioConfig(
      sampleRate: map['sampleRate'] ?? 44100,
      channels: map['channels'] ?? 2,
      bitsPerSample: map['bitsPerSample'] ?? 16,
    );
  }

  Map<String, dynamic> toMap() {
    return {
      'sampleRate': sampleRate,
      'channels': channels,
      'bitsPerSample': bitsPerSample,
    };
  }
}

abstract class WindowsLoopbackRecorderPlatform extends PlatformInterface {
  /// Constructs a WindowsLoopbackRecorderPlatform.
  WindowsLoopbackRecorderPlatform() : super(token: _token);

  static final Object _token = Object();

  static WindowsLoopbackRecorderPlatform _instance = MethodChannelWindowsLoopbackRecorder();

  /// The default instance of [WindowsLoopbackRecorderPlatform] to use.
  ///
  /// Defaults to [MethodChannelWindowsLoopbackRecorder].
  static WindowsLoopbackRecorderPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [WindowsLoopbackRecorderPlatform] when
  /// they register themselves.
  static set instance(WindowsLoopbackRecorderPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }

  /// Start recording system audio and microphone
  Future<bool> startRecording({AudioConfig? config}) {
    throw UnimplementedError('startRecording() has not been implemented.');
  }

  /// Pause the current recording
  Future<bool> pauseRecording() {
    throw UnimplementedError('pauseRecording() has not been implemented.');
  }

  /// Resume recording from paused state
  Future<bool> resumeRecording() {
    throw UnimplementedError('resumeRecording() has not been implemented.');
  }

  /// Stop recording and finalize
  Future<bool> stopRecording() {
    throw UnimplementedError('stopRecording() has not been implemented.');
  }

  /// Get current recording state
  Future<RecordingState> getRecordingState() {
    throw UnimplementedError('getRecordingState() has not been implemented.');
  }

  /// Check if microphone permission is granted
  Future<bool> hasMicrophonePermission() {
    throw UnimplementedError('hasMicrophonePermission() has not been implemented.');
  }

  /// Request microphone permission
  Future<bool> requestMicrophonePermission() {
    throw UnimplementedError('requestMicrophonePermission() has not been implemented.');
  }

  /// Get available audio devices
  Future<List<String>> getAvailableDevices() {
    throw UnimplementedError('getAvailableDevices() has not been implemented.');
  }

  /// Set up audio stream listener
  Stream<Uint8List> get audioStream {
    throw UnimplementedError('audioStream has not been implemented.');
  }

  /// Get actual audio format being used
  Future<AudioConfig> getAudioFormat() {
    throw UnimplementedError('getAudioFormat() has not been implemented.');
  }
}
