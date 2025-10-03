import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'windows_loopback_recorder_platform_interface.dart';

/// An implementation of [WindowsLoopbackRecorderPlatform] that uses method channels.
class MethodChannelWindowsLoopbackRecorder extends WindowsLoopbackRecorderPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('windows_loopback_recorder');

  /// The event channel used for audio stream data.
  @visibleForTesting
  final eventChannel = const EventChannel('windows_loopback_recorder/audio_stream');

  /// The event channel used for volume monitoring data.
  @visibleForTesting
  final volumeEventChannel = const EventChannel('windows_loopback_recorder/volume_stream');

  StreamSubscription<dynamic>? _audioStreamSubscription;
  final StreamController<Uint8List> _audioStreamController = StreamController<Uint8List>.broadcast();

  StreamSubscription<dynamic>? _volumeStreamSubscription;
  final StreamController<VolumeData> _volumeStreamController = StreamController<VolumeData>.broadcast();

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>('getPlatformVersion');
    return version;
  }

  @override
  Future<bool> startRecording({AudioConfig? config}) async {
    final result = await methodChannel.invokeMethod<bool>('startRecording', config?.toMap());
    if (result == true) {
      _setupAudioStream();
    }
    return result ?? false;
  }

  @override
  Future<bool> pauseRecording() async {
    final result = await methodChannel.invokeMethod<bool>('pauseRecording');
    return result ?? false;
  }

  @override
  Future<bool> resumeRecording() async {
    final result = await methodChannel.invokeMethod<bool>('resumeRecording');
    return result ?? false;
  }

  @override
  Future<bool> stopRecording() async {
    final result = await methodChannel.invokeMethod<bool>('stopRecording');
    await _audioStreamSubscription?.cancel();
    _audioStreamSubscription = null;
    return result ?? false;
  }

  @override
  Future<RecordingState> getRecordingState() async {
    final result = await methodChannel.invokeMethod<int>('getRecordingState');
    switch (result) {
      case 1:
        return RecordingState.recording;
      case 2:
        return RecordingState.paused;
      default:
        return RecordingState.idle;
    }
  }

  @override
  Future<bool> hasMicrophonePermission() async {
    final result = await methodChannel.invokeMethod<bool>('hasMicrophonePermission');
    return result ?? false;
  }

  @override
  Future<bool> requestMicrophonePermission() async {
    final result = await methodChannel.invokeMethod<bool>('requestMicrophonePermission');
    return result ?? false;
  }

  @override
  Future<List<String>> getAvailableDevices() async {
    final result = await methodChannel.invokeMethod<List<dynamic>>('getAvailableDevices');
    return result?.cast<String>() ?? [];
  }

  @override
  Stream<Uint8List> get audioStream => _audioStreamController.stream;

  @override
  Future<AudioConfig> getAudioFormat() async {
    try {
      final result = await methodChannel.invokeMethod('getAudioFormat');
      if (result != null && result is Map) {
        // Convert to Map<String, dynamic> safely
        final Map<String, dynamic> formatMap = {};
        result.forEach((key, value) {
          if (key is String) {
            formatMap[key] = value;
          }
        });
        return AudioConfig.fromMap(formatMap);
      }
    } catch (e) {
      debugPrint('Error getting audio format: $e');
    }
    return AudioConfig();
  }

  @override
  Future<bool> startVolumeMonitoring() async {
    final result = await methodChannel.invokeMethod<bool>('startVolumeMonitoring');
    if (result == true) {
      _setupVolumeStream();
    }
    return result ?? false;
  }

  @override
  Future<bool> stopVolumeMonitoring() async {
    final result = await methodChannel.invokeMethod<bool>('stopVolumeMonitoring');
    await _volumeStreamSubscription?.cancel();
    _volumeStreamSubscription = null;
    return result ?? false;
  }

  @override
  Stream<VolumeData> get volumeStream => _volumeStreamController.stream;

  void _setupAudioStream() {
    _audioStreamSubscription = eventChannel.receiveBroadcastStream().listen(
      (dynamic data) {
        if (data is Uint8List) {
          _audioStreamController.add(data);
        }
      },
      onError: (error) {
        debugPrint('Audio stream error: $error');
      },
    );
  }

  void _setupVolumeStream() {
    _volumeStreamSubscription = volumeEventChannel.receiveBroadcastStream().listen(
      (dynamic data) {
        try {
          if (data is Map) {
            // Convert to Map<String, dynamic> safely
            final Map<String, dynamic> volumeMap = {};
            data.forEach((key, value) {
              if (key is String) {
                volumeMap[key] = value;
              }
            });
            final volumeData = VolumeData.fromMap(volumeMap);
            _volumeStreamController.add(volumeData);
          }
        } catch (e) {
          debugPrint('Volume data parsing error: $e');
        }
      },
      onError: (error) {
        debugPrint('Volume stream error: $error');
      },
    );
  }

  void dispose() {
    _audioStreamSubscription?.cancel();
    _volumeStreamSubscription?.cancel();
    _audioStreamController.close();
    _volumeStreamController.close();
  }
}
