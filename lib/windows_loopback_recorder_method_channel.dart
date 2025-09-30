import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'windows_loopback_recorder_platform_interface.dart';

/// An implementation of [WindowsLoopbackRecorderPlatform] that uses method channels.
class MethodChannelWindowsLoopbackRecorder extends WindowsLoopbackRecorderPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('windows_loopback_recorder');

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>('getPlatformVersion');
    return version;
  }
}
