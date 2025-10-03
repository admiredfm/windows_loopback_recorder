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
      sampleRate: (map['sampleRate'] is int) ? map['sampleRate'] : 44100,
      channels: (map['channels'] is int) ? map['channels'] : 2,
      bitsPerSample: (map['bitsPerSample'] is int) ? map['bitsPerSample'] : 16,
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

/// Volume data from audio monitoring
class VolumeData {
  final double rms;        // Root Mean Square value (0.0 - 1.0)
  final double decibels;   // Decibel value (-96.0 dB - 0.0 dB)
  final int percentage;    // Percentage value (0% - 100%)
  final int timestamp;     // Timestamp in milliseconds

  const VolumeData({
    required this.rms,
    required this.decibels,
    required this.percentage,
    required this.timestamp,
  });

  factory VolumeData.fromMap(Map<String, dynamic> map) {
    return VolumeData(
      rms: (map['rms'] as num).toDouble(),
      decibels: (map['db'] as num).toDouble(),
      percentage: map['percentage'] as int,
      timestamp: map['timestamp'] as int,
    );
  }

  @override
  String toString() {
    return 'VolumeData(rms: ${rms.toStringAsFixed(3)}, '
           'db: ${decibels.toStringAsFixed(1)}dB, '
           'percentage: $percentage%, '
           'timestamp: $timestamp)';
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

  /// Start volume monitoring
  Future<bool> startVolumeMonitoring() {
    throw UnimplementedError('startVolumeMonitoring() has not been implemented.');
  }

  /// Stop volume monitoring
  Future<bool> stopVolumeMonitoring() {
    throw UnimplementedError('stopVolumeMonitoring() has not been implemented.');
  }

  /// Volume stream
  Stream<VolumeData> get volumeStream {
    throw UnimplementedError('volumeStream has not been implemented.');
  }
}
