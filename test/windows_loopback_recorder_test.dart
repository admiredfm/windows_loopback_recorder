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
