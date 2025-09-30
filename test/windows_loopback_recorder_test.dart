import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:windows_loopback_recorder/windows_loopback_recorder.dart';
import 'package:windows_loopback_recorder/windows_loopback_recorder_platform_interface.dart';
import 'package:windows_loopback_recorder/windows_loopback_recorder_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockWindowsLoopbackRecorderPlatform
    with MockPlatformInterfaceMixin
    implements WindowsLoopbackRecorderPlatform {

  @override
  Future<String?> getPlatformVersion() => Future.value('42');

  @override
  Future<bool> startRecording({AudioConfig? config}) => Future.value(true);

  @override
  Future<bool> pauseRecording() => Future.value(true);

  @override
  Future<bool> resumeRecording() => Future.value(true);

  @override
  Future<bool> stopRecording() => Future.value(true);

  @override
  Future<RecordingState> getRecordingState() => Future.value(RecordingState.idle);

  @override
  Future<bool> hasMicrophonePermission() => Future.value(true);

  @override
  Future<bool> requestMicrophonePermission() => Future.value(true);

  @override
  Future<List<String>> getAvailableDevices() => Future.value(['Mock Device']);

  @override
  Stream<Uint8List> get audioStream => Stream.fromIterable([Uint8List(0)]);

  @override
  Future<AudioConfig> getAudioFormat() => Future.value(AudioConfig());
}

void main() {
  final WindowsLoopbackRecorderPlatform initialPlatform = WindowsLoopbackRecorderPlatform.instance;

  test('$MethodChannelWindowsLoopbackRecorder is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelWindowsLoopbackRecorder>());
  });

  test('getPlatformVersion', () async {
    WindowsLoopbackRecorder windowsLoopbackRecorderPlugin = WindowsLoopbackRecorder();
    MockWindowsLoopbackRecorderPlatform fakePlatform = MockWindowsLoopbackRecorderPlatform();
    WindowsLoopbackRecorderPlatform.instance = fakePlatform;

    expect(await windowsLoopbackRecorderPlugin.getPlatformVersion(), '42');
  });
}
