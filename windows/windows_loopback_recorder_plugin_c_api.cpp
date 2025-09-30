#include "windows_loopback_recorder/windows_loopback_recorder_plugin.h"

#include <flutter/plugin_registrar_windows.h>

void WindowsLoopbackRecorderPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  windows_loopback_recorder::WindowsLoopbackRecorderPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
