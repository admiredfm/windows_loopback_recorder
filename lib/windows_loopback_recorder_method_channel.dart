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

  StreamSubscription<dynamic>? _audioStreamSubscription;
  final StreamController<Uint8List> _audioStreamController = StreamController<Uint8List>.broadcast();

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

  void dispose() {
    _audioStreamSubscription?.cancel();
    _audioStreamController.close();
  }
}
