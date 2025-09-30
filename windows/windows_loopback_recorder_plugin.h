#ifndef FLUTTER_PLUGIN_WINDOWS_LOOPBACK_RECORDER_PLUGIN_H_
#define FLUTTER_PLUGIN_WINDOWS_LOOPBACK_RECORDER_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

namespace windows_loopback_recorder {

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
};

}  // namespace windows_loopback_recorder

#endif  // FLUTTER_PLUGIN_WINDOWS_LOOPBACK_RECORDER_PLUGIN_H_
