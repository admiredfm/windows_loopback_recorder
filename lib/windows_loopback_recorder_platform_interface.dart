import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'windows_loopback_recorder_method_channel.dart';

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
}
